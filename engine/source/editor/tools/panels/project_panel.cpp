#include "project_panel.h"

#include "editor/core/editor_context.h"

#include <algorithm>
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

            if (!m_showMetaFiles && isMetaFile(p))
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

    void ProjectPanel::renderToolbar(EditorContext& ctx)
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
        ImGui::SameLine();

        if (ImGui::Button("Up"))
        {
            if (!m_assetsRoot.empty() && m_currentDir != m_assetsRoot)
            {
                m_currentDir = m_currentDir.parent_path();
                gatherEntries(m_currentDir);
                m_selectedRelStr.clear();
            }
        }
        ImGui::SameLine();

        if (ImGui::Button("Refresh"))
        {
            gatherEntries(m_currentDir);
        }
        ImGui::SameLine();

        if (ImGui::Button("New Folder"))
        {
            std::strncpy(m_newFolderName, "NewFolder", sizeof(m_newFolderName) - 1);
            m_newFolderName[sizeof(m_newFolderName) - 1] = '\0';
            m_openCreateFolderPopup = true;
        }
        ImGui::SameLine();

        ImGui::Checkbox("Show .meta", &m_showMetaFiles);

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

    void ProjectPanel::renderDirectoryTree(EditorContext&)
    {
        if (m_assetsRoot.empty())
        {
            ImGui::TextUnformatted("Assets root not found.");
            return;
        }

        std::function<void(const std::filesystem::path&)> drawNode;
        drawNode = [&](const std::filesystem::path& dir) {
            const auto name = (dir == m_assetsRoot) ? std::string("Assets") : dir.filename().string();

            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanFullWidth;
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

            const bool opened = ImGui::TreeNodeEx(name.c_str(), flags);

            if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
            {
                m_currentDir = dir;
                gatherEntries(m_currentDir);
                m_selectedRelStr.clear();
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
    }

    void ProjectPanel::renderContent(EditorContext& ctx)
    {
        if (m_assetsRoot.empty())
            return;

        ImGui::Separator();

        for (const auto& e : m_entries)
        {
            const std::string name = e.physical.filename().string();
            const std::string relStr = e.rel.empty() ? std::string{} : e.rel.generic_string();

            const char* prefix = e.is_dir ? "[DIR] " : "      ";

            const bool selected = (!relStr.empty() && relStr == m_selectedRelStr);
            if (ImGui::Selectable((std::string(prefix) + name).c_str(), selected))
            {
                m_selectedRelStr = relStr;
            }

            if (e.is_dir && ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            {
                m_currentDir = e.physical;
                gatherEntries(m_currentDir);
                m_selectedRelStr.clear();
            }

            if (ImGui::BeginPopupContextItem())
            {
                ImGui::TextUnformatted(name.c_str());
                ImGui::Separator();

                if (!e.is_dir && !e.rel.empty())
                {
                    const auto vpath = relToAssetVPath(e.rel);
                    ImGui::Text("VPath: %s", vpath.c_str());
                }

                if (ImGui::MenuItem("Rename"))
                {
                    m_renameFrom = e.physical;
                    std::string old_name = e.physical.filename().string();
                    std::strncpy(m_renameInput, old_name.c_str(), sizeof(m_renameInput) - 1);
                    m_renameInput[sizeof(m_renameInput) - 1] = '\0';
                    m_openRenamePopup = true;
                }

                if (ImGui::MenuItem("Delete"))
                {
                    if (deleteEntry(ctx, e))
                    {
                        gatherEntries(m_currentDir);
                        if (m_selectedRelStr == relStr)
                            m_selectedRelStr.clear();
                    }
                }

                if (ImGui::MenuItem("Refresh"))
                {
                    gatherEntries(m_currentDir);
                }

                ImGui::EndPopup();
            }
        }

        renderRenamePopup(ctx);
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

        renderToolbar(ctx);
        ImGui::Separator();

        ImGui::Columns(2, "ProjectColumns", true);

        ImGui::BeginChild("##ProjectTree");
        renderDirectoryTree(ctx);
        ImGui::EndChild();

        ImGui::NextColumn();

        ImGui::BeginChild("##ProjectContent");
        renderContent(ctx);
        ImGui::EndChild();

        ImGui::Columns(1);

        ImGui::End();
    }
} // namespace Hybrid


