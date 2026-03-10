#include "project_panel.h"

#include "editor/core/editor_context.h"
#include "editor/platform/windows/file_dialogs_win32.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <functional>
#include <system_error>

#include <imgui.h>

#include "runtime/core/base/macro.h"
#include "runtime/modules/project/project_context.h"

namespace Hybrid
{
    void ProjectPanel::ensureRootInit()
    {
        if (m_rootInited)
            return;

        const auto& proj = ProjectService::Get();
        m_assetsRoot = proj.assets;

        if (m_assetsRoot.empty() || !std::filesystem::exists(m_assetsRoot))
        {
            HBD_CORE_WARN("ProjectPanel: assets root invalid: {}", m_assetsRoot.string());
            m_assetsRoot.clear();
            m_currentDir.clear();
            m_entries.clear();
            m_rootInited = true;
            return;
        }

        m_currentDir = m_assetsRoot;
        gatherEntries(m_currentDir);
        m_rootInited = true;
    }

    void ProjectPanel::drawCommonContextMenu(EditorContext& ctx, const Entry* target)
    {
        const bool has_target = (target != nullptr);
        const bool is_dir = has_target && target->is_dir;
        const bool can_open = has_target && (is_dir || target->rel.extension() == ".scene");
        const bool can_copy_asset_path = has_target && !target->rel.empty();
        const bool can_duplicate = has_target;
        const bool can_show_in_explorer = has_target || !m_currentDir.empty();
        const bool can_new_folder_here = (!has_target || is_dir);
        const bool can_rename = has_target;
        const bool can_delete = has_target;

        if (has_target)
        {
            ImGui::TextUnformatted(target->physical.filename().string().c_str());
            ImGui::Separator();
        }

        if (ImGui::MenuItem("Open", nullptr, false, can_open))
        {
            if (openEntry(ctx, *target))
                m_selectedRelStr.clear();
        }

        if (ImGui::BeginMenu("Create"))
        {
            if (ImGui::MenuItem("Folder", nullptr, false, can_new_folder_here))
                openCreateFolderPopup(has_target ? target->physical : m_currentDir);
            ImGui::EndMenu();
        }

        ImGui::Separator();

        if (ImGui::MenuItem("Rename", nullptr, false, can_rename))
        {
            m_renameFrom = target->physical;
            std::string old_name = target->physical.filename().string();
            std::strncpy(m_renameInput, old_name.c_str(), sizeof(m_renameInput) - 1);
            m_renameInput[sizeof(m_renameInput) - 1] = '\0';
            m_openRenamePopup = true;
        }

        if (ImGui::MenuItem("Duplicate", nullptr, false, can_duplicate))
        {
            if (duplicateEntry(ctx, *target))
                gatherEntries(m_currentDir);
        }

        ImGui::Separator();

        if (ImGui::MenuItem("Show in Explorer", nullptr, false, can_show_in_explorer))
            (void)ShowInExplorerWin32(has_target ? target->physical : m_currentDir);

        if (ImGui::MenuItem("Copy Asset Path", nullptr, false, can_copy_asset_path))
        {
            const auto vpath = relToAssetVPath(target->rel);
            ImGui::SetClipboardText(vpath.c_str());
        }

        if (ImGui::MenuItem("Delete", nullptr, false, can_delete))
        {
            if (deleteEntry(ctx, *target))
            {
                gatherEntries(m_currentDir);
                if (!target->rel.empty() && m_selectedRelStr == target->rel.generic_string())
                    m_selectedRelStr.clear();
            }
        }

        ImGui::Separator();

        if (ImGui::MenuItem("Refresh"))
            gatherEntries(m_currentDir);
    }

    bool ProjectPanel::isMetaFile(const std::filesystem::path& p) const
    {
        return p.extension() == ".meta";
    }

    bool ProjectPanel::isHiddenFile(const std::filesystem::path& p) const
    {
        const auto name = p.filename().string();
        return !name.empty() && name[0] == '.';
    }

    std::string ProjectPanel::relToAssetVPath(const std::filesystem::path& rel) const
    {
        std::string r = rel.generic_string();
        while (!r.empty() && (r.front() == '/' || r.front() == '\\'))
            r.erase(r.begin());
        return std::string("asset:") + r;
    }

    void ProjectPanel::notifyAssetChange(EditorContext& ctx, const std::filesystem::path& rel, bool removed) const
    {
        if (rel.empty() || !ctx.notify_asset_source_event)
            return;

        const auto ext = rel.extension();
        if (ext == ".meta")
            return;

        AssetSourceEvent event{};
        event.type = removed ? AssetSourceEventType::Removed : AssetSourceEventType::Added;
        event.path = relToAssetVPath(rel);
        ctx.notify_asset_source_event(event);
    }

    void ProjectPanel::notifyAssetChangeRecursive(EditorContext& ctx,
                                                  const std::filesystem::path& root,
                                                  bool removed,
                                                  std::vector<std::filesystem::path>* out_rel_files) const
    {
        if (root.empty() || !std::filesystem::exists(root))
            return;

        std::error_code ec;
        std::filesystem::recursive_directory_iterator it(root, ec), end;
        if (ec)
            return;

        for (; it != end; it.increment(ec))
        {
            if (ec)
            {
                ec.clear();
                continue;
            }
            if (!it->is_regular_file())
                continue;
            if (isHiddenFile(it->path()))
                continue;

            auto rel = std::filesystem::relative(it->path(), m_assetsRoot, ec);
            if (ec)
            {
                ec.clear();
                continue;
            }

            if (out_rel_files)
            {
                out_rel_files->push_back(rel);
            }
            else
            {
                notifyAssetChange(ctx, rel, removed);
            }
        }
    }

    bool ProjectPanel::createFolder(const std::string& folder_name)
    {
        if (m_currentDir.empty() || folder_name.empty())
            return false;

        std::filesystem::path target = m_currentDir / folder_name;
        std::error_code ec;
        if (std::filesystem::exists(target))
            return false;

        const bool ok = std::filesystem::create_directories(target, ec);
        return ok && !ec;
    }

    void ProjectPanel::openCreateFolderPopup()
    {
        openCreateFolderPopup(m_currentDir);
    }

    void ProjectPanel::openCreateFolderPopup(std::filesystem::path target_dir)
    {
        if (!target_dir.empty())
            m_currentDir = std::move(target_dir);

        std::strncpy(m_newFolderName, "NewFolder", sizeof(m_newFolderName) - 1);
        m_newFolderName[sizeof(m_newFolderName) - 1] = '\0';
        m_openCreateFolderPopup = true;
    }

    bool ProjectPanel::openEntry(EditorContext& ctx, const Entry& e)
    {
        if (e.physical.empty() || !std::filesystem::exists(e.physical))
            return false;

        if (e.is_dir)
        {
            m_currentDir = e.physical;
            gatherEntries(m_currentDir);
            m_selectedRelStr.clear();
            return true;
        }

        if (e.rel.extension() == ".scene")
        {
            const auto vpath = relToAssetVPath(e.rel);
            if (!ctx.open_scene)
            {
                HBD_CORE_WARN("ProjectPanel: ctx.open_scene not bound, cannot open {}", vpath);
                return false;
            }

            ctx.open_scene(vpath);
            return true;
        }

        return false;
    }

    bool ProjectPanel::deleteEntry(EditorContext& ctx, const Entry& e)
    {
        if (e.physical.empty() || !std::filesystem::exists(e.physical))
            return false;

        std::error_code ec;
        if (e.is_dir)
        {
            notifyAssetChangeRecursive(ctx, e.physical, true, nullptr);
            std::filesystem::remove_all(e.physical, ec);
            if (ec)
                return false;
            return true;
        }

        const auto rel = e.rel;
        std::filesystem::remove(e.physical, ec);
        if (ec)
            return false;

        notifyAssetChange(ctx, rel, true);
        return true;
    }

    bool ProjectPanel::duplicateEntry(EditorContext& ctx, const Entry& e)
    {
        if (e.physical.empty() || !std::filesystem::exists(e.physical))
            return false;

        const auto parent = e.physical.parent_path();
        const auto stem = e.physical.stem().string();
        const auto ext = e.physical.extension().string();

        std::filesystem::path target;
        std::error_code ec;
        for (int index = 1; index <= 999; ++index)
        {
            std::string candidate_name;
            if (e.is_dir)
            {
                candidate_name = (index == 1)
                    ? (e.physical.filename().string() + " Copy")
                    : (e.physical.filename().string() + " Copy " + std::to_string(index));
            }
            else
            {
                candidate_name = (index == 1)
                    ? (stem + " Copy" + ext)
                    : (stem + " Copy " + std::to_string(index) + ext);
            }

            target = parent / candidate_name;
            if (!std::filesystem::exists(target, ec) && !ec)
                break;
            ec.clear();
            target.clear();
        }

        if (target.empty())
            return false;

        if (e.is_dir)
        {
            std::filesystem::copy(e.physical,
                                  target,
                                  std::filesystem::copy_options::recursive,
                                  ec);
            if (ec)
                return false;

            notifyAssetChangeRecursive(ctx, target, false, nullptr);
            return true;
        }

        std::filesystem::copy_file(e.physical, target, std::filesystem::copy_options::none, ec);
        if (ec)
            return false;

        auto rel = std::filesystem::relative(target, m_assetsRoot, ec);
        if (ec)
            return false;

        notifyAssetChange(ctx, rel, false);
        return true;
    }

    bool ProjectPanel::renameEntry(EditorContext& ctx,
                                   const std::filesystem::path& from,
                                   const std::filesystem::path& to)
    {
        if (from.empty() || to.empty() || from == to)
            return false;

        std::error_code ec;
        if (!std::filesystem::exists(from) || std::filesystem::exists(to))
            return false;

        const bool from_is_dir = std::filesystem::is_directory(from, ec);
        if (ec)
            return false;

        auto notifyAssetMove = [this, &ctx](const std::filesystem::path& old_rel,
                                            const std::filesystem::path& new_rel) {
            if (old_rel.empty() || new_rel.empty())
                return;
            if (old_rel.extension() == ".meta" || new_rel.extension() == ".meta")
                return;

            if (ctx.notify_asset_source_event)
            {
                AssetSourceEvent event{};
                event.type = AssetSourceEventType::Moved;
                event.old_path = relToAssetVPath(old_rel);
                event.new_path = relToAssetVPath(new_rel);
                ctx.notify_asset_source_event(event);
                return;
            }

            // Backward-compatible fallback.
            notifyAssetChange(ctx, old_rel, true);
            notifyAssetChange(ctx, new_rel, false);
        };

        std::vector<std::filesystem::path> old_files;
        if (from_is_dir)
        {
            notifyAssetChangeRecursive(ctx, from, false, &old_files);
        }

        std::filesystem::rename(from, to, ec);
        if (ec)
            return false;

        if (from_is_dir)
        {
            for (const auto& old_rel : old_files)
            {
                std::filesystem::path old_abs = m_assetsRoot / old_rel;
                auto sub = std::filesystem::relative(old_abs, from, ec);
                if (ec)
                {
                    ec.clear();
                    continue;
                }

                const auto new_abs = to / sub;
                auto new_rel = std::filesystem::relative(new_abs, m_assetsRoot, ec);
                if (ec)
                {
                    ec.clear();
                    continue;
                }

                notifyAssetMove(old_rel, new_rel);
            }
            return true;
        }

        auto old_rel = std::filesystem::relative(from, m_assetsRoot, ec);
        if (ec)
            return false;

        ec.clear();
        auto new_rel = std::filesystem::relative(to, m_assetsRoot, ec);
        if (ec)
            return false;

        notifyAssetMove(old_rel, new_rel);

        return true;
    }

    void ProjectPanel::gatherEntries(const std::filesystem::path& dir)
    {
        m_entries.clear();

        if (m_assetsRoot.empty() || dir.empty())
            return;

        std::error_code ec;
        std::filesystem::directory_iterator it(dir, ec), end;
        if (ec)
        {
            HBD_CORE_WARN("ProjectPanel: cannot list dir {} ({})", dir.string(), ec.message());
            return;
        }

        for (; it != end; it.increment(ec))
        {
            if (ec)
            {
                ec.clear();
                continue;
            }

            const auto p = it->path();
            const bool is_dir = it->is_directory();

            if (isHiddenFile(p))
                continue;

            if (isMetaFile(p))
                continue;

            if (!is_dir && !it->is_regular_file())
                continue;

            Entry e{};
            e.physical = p;
            e.is_dir = is_dir;

            std::error_code rec;
            e.rel = std::filesystem::relative(p, m_assetsRoot, rec);
            if (rec)
                e.rel.clear();

            m_entries.push_back(std::move(e));
        }

        std::sort(m_entries.begin(), m_entries.end(), [](const Entry& a, const Entry& b) {
            if (a.is_dir != b.is_dir)
                return a.is_dir > b.is_dir;
            return a.physical.filename().string() < b.physical.filename().string();
        });
    }

    void ProjectPanel::renderPathHeader() const
    {
        std::string pathLabel = "Assets";
        if (!m_assetsRoot.empty() && !m_currentDir.empty())
        {
            std::error_code ec;
            auto rel = std::filesystem::relative(m_currentDir, m_assetsRoot, ec);
            if (!ec && !rel.empty() && rel != ".")
                pathLabel += "/" + rel.generic_string();
        }

        ImGui::TextUnformatted(pathLabel.c_str());
    }

    void ProjectPanel::renderCreateFolderPopup()
    {
        if (m_openCreateFolderPopup)
        {
            ImGui::OpenPopup("Create Folder");
            m_openCreateFolderPopup = false;
        }

        if (ImGui::BeginPopupModal("Create Folder", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::InputText("Name", m_newFolderName, IM_ARRAYSIZE(m_newFolderName));

            if (ImGui::Button("Create"))
            {
                if (createFolder(m_newFolderName))
                {
                    gatherEntries(m_currentDir);
                }
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel"))
            {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }

    void ProjectPanel::renderDirectoryTree(EditorContext& ctx)
    {
        if (m_assetsRoot.empty())
        {
            ImGui::TextUnformatted("Assets root not found.");
            return;
        }

        std::function<void(const std::filesystem::path&)> drawNode;
        drawNode = [&](const std::filesystem::path& dir) {
            const auto name = (dir == m_assetsRoot) ? std::string("Assets") : dir.filename().string();
            Entry entry{};
            entry.physical = dir;
            entry.is_dir = true;
            std::error_code rel_ec;
            entry.rel = std::filesystem::relative(dir, m_assetsRoot, rel_ec);
            if (rel_ec)
                entry.rel.clear();

            ImGuiTreeNodeFlags flags =
                ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanFullWidth;
            if (dir == m_currentDir)
                flags |= ImGuiTreeNodeFlags_Selected;

            bool hasChildDir = false;
            {
                std::error_code ec;
                for (auto it = std::filesystem::directory_iterator(dir, ec);
                     it != std::filesystem::directory_iterator();
                     it.increment(ec))
                {
                    if (ec)
                    {
                        ec.clear();
                        continue;
                    }
                    if (it->is_directory())
                    {
                        const auto child = it->path();
                        if (isHiddenFile(child))
                            continue;
                        hasChildDir = true;
                        break;
                    }
                }
            }
            if (!hasChildDir)
                flags |= ImGuiTreeNodeFlags_Leaf;

            const auto node_hash = std::hash<std::filesystem::path>{}(dir);
            const bool opened =
                ImGui::TreeNodeEx(reinterpret_cast<const void*>(static_cast<uintptr_t>(node_hash)), flags, "%s", name.c_str());

            if (ImGui::IsItemClicked())
            {
                m_currentDir = dir;
                gatherEntries(m_currentDir);
                m_selectedRelStr.clear();
            }

            const bool tree_double_clicked =
                ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
            if (tree_double_clicked)
                (void)openEntry(ctx, entry);

            if (ImGui::BeginPopupContextItem())
            {
                drawCommonContextMenu(ctx, &entry);
                ImGui::EndPopup();
            }

            if (opened)
            {
                std::error_code ec;
                std::vector<std::filesystem::path> children;
                for (auto it = std::filesystem::directory_iterator(dir, ec);
                     it != std::filesystem::directory_iterator();
                     it.increment(ec))
                {
                    if (ec)
                    {
                        ec.clear();
                        continue;
                    }
                    if (!it->is_directory())
                        continue;
                    const auto child = it->path();
                    if (isHiddenFile(child))
                        continue;
                    children.push_back(child);
                }

                std::sort(children.begin(), children.end(), [](const auto& a, const auto& b) {
                    return a.filename().string() < b.filename().string();
                });

                for (const auto& c : children)
                    drawNode(c);

                ImGui::TreePop();
            }
        };

        drawNode(m_assetsRoot);
        drawWindowContextMenuIfRequested(ctx, "##ProjectTreeContext");
    }

    void ProjectPanel::renderContent(EditorContext& ctx)
    {
        if (m_assetsRoot.empty())
            return;

        bool request_refresh = false;
        bool request_clear_selection = false;

        const std::vector<Entry> entries_snapshot = m_entries;
        for (const auto& e : entries_snapshot)
        {
            const std::string name = e.physical.filename().string();
            const std::string relStr = e.rel.empty() ? std::string{} : e.rel.generic_string();

            const char* prefix = e.is_dir ? "[DIR] " : "      ";

            const bool selected = (!relStr.empty() && relStr == m_selectedRelStr);
            const bool clicked =
                ImGui::Selectable((std::string(prefix) + name).c_str(), selected, ImGuiSelectableFlags_AllowDoubleClick);
            if (clicked)
            {
                m_selectedRelStr = relStr;
            }

            const bool double_clicked = ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);

            if (double_clicked && openEntry(ctx, e))
            {
                request_refresh = e.is_dir;
                request_clear_selection = true;
                break;
            }

            if (ImGui::BeginPopupContextItem())
            {
                drawCommonContextMenu(ctx, &e);
                ImGui::EndPopup();
            }
        }

        if (request_refresh)
            gatherEntries(m_currentDir);

        if (request_clear_selection)
            m_selectedRelStr.clear();

        drawWindowContextMenuIfRequested(ctx, "##ProjectContentContext");
        renderRenamePopup(ctx);
    }

    void ProjectPanel::drawWindowContextMenu(EditorContext& ctx)
    {
        drawCommonContextMenu(ctx, nullptr);
    }

    void ProjectPanel::renderRenamePopup(EditorContext& ctx)
    {
        if (m_openRenamePopup)
        {
            ImGui::OpenPopup("Rename Entry");
            m_openRenamePopup = false;
        }

        if (!ImGui::BeginPopupModal("Rename Entry", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
            return;

        ImGui::InputText("New Name", m_renameInput, IM_ARRAYSIZE(m_renameInput));

        if (ImGui::Button("OK"))
        {
            if (!m_renameFrom.empty())
            {
                const std::filesystem::path to = m_renameFrom.parent_path() / std::string(m_renameInput);
                if (renameEntry(ctx, m_renameFrom, to))
                {
                    gatherEntries(m_currentDir);
                    m_selectedRelStr.clear();
                }
            }
            m_renameFrom.clear();
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
        {
            m_renameFrom.clear();
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    void ProjectPanel::onImGuiRender(EditorContext& ctx)
    {
        if (!m_open)
            return;

        ensureRootInit();

        if (!m_currentDir.empty())
        {
            const auto now = std::chrono::steady_clock::now();
            if (m_lastAutoRefresh.time_since_epoch().count() == 0)
            {
                m_lastAutoRefresh = now;
            }
            else
            {
                const auto elapsed = std::chrono::duration<float>(now - m_lastAutoRefresh).count();
                if (elapsed >= m_autoRefreshIntervalSec)
                {
                    gatherEntries(m_currentDir);
                    m_lastAutoRefresh = now;
                }
            }
        }

        ImGui::Begin(getName(), &m_open);

        const float project_panel_width = ImGui::GetContentRegionAvail().x;
        ImGui::Columns(2, "ProjectColumns", true);
        if (!m_columnsInitialized)
        {
            ImGui::SetColumnWidth(0, project_panel_width * 0.25f);
            m_columnsInitialized = true;
        }

        ImGui::BeginChild("##ProjectTree");
        renderDirectoryTree(ctx);
        ImGui::EndChild();

        ImGui::NextColumn();

        ImGui::BeginChild("##ProjectContent");
        renderPathHeader();
        ImGui::Separator();
        renderContent(ctx);
        ImGui::EndChild();

        ImGui::Columns(1);

        renderCreateFolderPopup();
        ImGui::End();
    }
} // namespace Hybrid


