#include "project_panel.h"

#include "editor/core/context/editor_context.h"
#include "editor/core/editor_drag_drop.h"

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
    namespace
    {
        constexpr const char* kProjectPanelLogTag = "[ProjectPanel]";

        std::string pathOrPlaceholder(const std::filesystem::path& path)
        {
            return path.empty() ? std::string("<empty>") : path.generic_string();
        }
    } // namespace

    void ProjectPanel::ensureRootInit()
    {
        if (m_rootInited)
            return;

        const auto& proj = ProjectService::Get();
        m_assetsRoot = proj.assets;

        if (m_assetsRoot.empty() || !std::filesystem::exists(m_assetsRoot))
        {
            HBD_CORE_WARN("{} assets_root_invalid path={}", kProjectPanelLogTag, pathOrPlaceholder(m_assetsRoot));
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
        const bool can_reimport = has_target && !is_dir && !target->rel.empty() && ctx.asset_actions.request_reimport_asset;
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
            if (ImGui::MenuItem("Scene"))
            {
                if (ctx.documents.request_new_scene)
                    (void)ctx.documents.request_new_scene();
            }
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

        if (ImGui::MenuItem("Reimport", nullptr, false, can_reimport))
        {
            if (ctx.asset_actions.request_reimport_asset)
                (void)ctx.asset_actions.request_reimport_asset(relToAssetVPath(target->rel));
        }

        ImGui::Separator();

        if (ImGui::MenuItem("Show in Explorer", nullptr, false, can_show_in_explorer))
        {
            if (ctx.documents.reveal_in_file_browser)
                (void)ctx.documents.reveal_in_file_browser(has_target ? target->physical : m_currentDir);
        }

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
        if (rel.empty() || !ctx.asset_actions.notify_asset_source_event)
            return;

        const auto ext = rel.extension();
        if (ext == ".meta")
            return;

        AssetSourceEvent event{};
        event.type = removed ? AssetSourceEventType::Removed : AssetSourceEventType::Added;
        event.path = relToAssetVPath(rel);
        ctx.asset_actions.notify_asset_source_event(event);
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
        {
            HBD_CORE_WARN("{} create_folder_rejected current_dir={} folder_name={}",
                          kProjectPanelLogTag,
                          pathOrPlaceholder(m_currentDir),
                          folder_name.empty() ? "<empty>" : folder_name);
            return false;
        }

        std::filesystem::path target = m_currentDir / folder_name;
        std::error_code ec;
        if (std::filesystem::exists(target))
        {
            HBD_CORE_WARN("{} create_folder_failed path={} reason=already_exists",
                          kProjectPanelLogTag,
                          pathOrPlaceholder(target));
            return false;
        }

        const bool ok = std::filesystem::create_directories(target, ec);
        if (!ok || ec)
        {
            HBD_CORE_WARN("{} create_folder_failed path={} reason={}",
                          kProjectPanelLogTag,
                          pathOrPlaceholder(target),
                          ec ? ec.message() : "create_directories_returned_false");
            return false;
        }

        HBD_CORE_INFO("{} create_folder_completed path={}", kProjectPanelLogTag, pathOrPlaceholder(target));
        return true;
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
        {
            HBD_CORE_WARN("{} open_rejected path={} reason=missing_entry",
                          kProjectPanelLogTag,
                          pathOrPlaceholder(e.physical));
            return false;
        }

        if (e.is_dir)
        {
            m_currentDir = e.physical;
            gatherEntries(m_currentDir);
            m_selectedRelStr.clear();
            HBD_CORE_INFO("{} directory_opened path={}", kProjectPanelLogTag, pathOrPlaceholder(e.physical));
            return true;
        }

        if (e.rel.extension() == ".scene")
        {
            const auto vpath = relToAssetVPath(e.rel);
            if (!ctx.documents.open_scene)
            {
                HBD_CORE_WARN("{} open_scene_rejected path={} reason=open_scene_not_bound",
                              kProjectPanelLogTag,
                              vpath);
                return false;
            }

            HBD_CORE_INFO("{} open_scene_requested path={}", kProjectPanelLogTag, vpath);
            ctx.documents.open_scene(vpath);
            return true;
        }

        return false;
    }

    bool ProjectPanel::deleteEntry(EditorContext& ctx, const Entry& e)
    {
        if (e.physical.empty() || !std::filesystem::exists(e.physical))
        {
            HBD_CORE_WARN("{} delete_rejected path={} reason=missing_entry",
                          kProjectPanelLogTag,
                          pathOrPlaceholder(e.physical));
            return false;
        }

        std::error_code ec;
        if (e.is_dir)
        {
            notifyAssetChangeRecursive(ctx, e.physical, true, nullptr);
            std::filesystem::remove_all(e.physical, ec);
            if (ec)
            {
                HBD_CORE_WARN("{} delete_failed path={} reason={}",
                              kProjectPanelLogTag,
                              pathOrPlaceholder(e.physical),
                              ec.message());
                return false;
            }
            HBD_CORE_INFO("{} delete_completed path={} entry_type=directory",
                          kProjectPanelLogTag,
                          pathOrPlaceholder(e.physical));
            return true;
        }

        const auto rel = e.rel;
        std::filesystem::remove(e.physical, ec);
        if (ec)
        {
            HBD_CORE_WARN("{} delete_failed path={} reason={}",
                          kProjectPanelLogTag,
                          pathOrPlaceholder(e.physical),
                          ec.message());
            return false;
        }

        notifyAssetChange(ctx, rel, true);
        HBD_CORE_INFO("{} delete_completed path={} entry_type=file",
                      kProjectPanelLogTag,
                      pathOrPlaceholder(e.physical));
        return true;
    }

    bool ProjectPanel::duplicateEntry(EditorContext& ctx, const Entry& e)
    {
        if (e.physical.empty() || !std::filesystem::exists(e.physical))
        {
            HBD_CORE_WARN("{} duplicate_rejected path={} reason=missing_entry",
                          kProjectPanelLogTag,
                          pathOrPlaceholder(e.physical));
            return false;
        }

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
        {
            HBD_CORE_WARN("{} duplicate_failed path={} reason=no_available_target",
                          kProjectPanelLogTag,
                          pathOrPlaceholder(e.physical));
            return false;
        }

        if (e.is_dir)
        {
            std::filesystem::copy(e.physical,
                                  target,
                                  std::filesystem::copy_options::recursive,
                                  ec);
            if (ec)
            {
                HBD_CORE_WARN("{} duplicate_failed source={} target={} reason={}",
                              kProjectPanelLogTag,
                              pathOrPlaceholder(e.physical),
                              pathOrPlaceholder(target),
                              ec.message());
                return false;
            }

            notifyAssetChangeRecursive(ctx, target, false, nullptr);
            HBD_CORE_INFO("{} duplicate_completed source={} target={} entry_type=directory",
                          kProjectPanelLogTag,
                          pathOrPlaceholder(e.physical),
                          pathOrPlaceholder(target));
            return true;
        }

        std::filesystem::copy_file(e.physical, target, std::filesystem::copy_options::none, ec);
        if (ec)
        {
            HBD_CORE_WARN("{} duplicate_failed source={} target={} reason={}",
                          kProjectPanelLogTag,
                          pathOrPlaceholder(e.physical),
                          pathOrPlaceholder(target),
                          ec.message());
            return false;
        }

        auto rel = std::filesystem::relative(target, m_assetsRoot, ec);
        if (ec)
        {
            HBD_CORE_WARN("{} duplicate_failed source={} target={} reason={}",
                          kProjectPanelLogTag,
                          pathOrPlaceholder(e.physical),
                          pathOrPlaceholder(target),
                          ec.message());
            return false;
        }

        notifyAssetChange(ctx, rel, false);
        HBD_CORE_INFO("{} duplicate_completed source={} target={} entry_type=file",
                      kProjectPanelLogTag,
                      pathOrPlaceholder(e.physical),
                      pathOrPlaceholder(target));
        return true;
    }

    bool ProjectPanel::renameEntry(EditorContext& ctx,
                                   const std::filesystem::path& from,
                                   const std::filesystem::path& to)
    {
        if (from.empty() || to.empty() || from == to)
        {
            HBD_CORE_WARN("{} rename_rejected from={} to={}",
                          kProjectPanelLogTag,
                          pathOrPlaceholder(from),
                          pathOrPlaceholder(to));
            return false;
        }

        std::error_code ec;
        if (!std::filesystem::exists(from) || std::filesystem::exists(to))
        {
            HBD_CORE_WARN("{} rename_rejected from={} to={} reason=invalid_path_state",
                          kProjectPanelLogTag,
                          pathOrPlaceholder(from),
                          pathOrPlaceholder(to));
            return false;
        }

        const bool from_is_dir = std::filesystem::is_directory(from, ec);
        if (ec)
        {
            HBD_CORE_WARN("{} rename_failed from={} to={} reason={}",
                          kProjectPanelLogTag,
                          pathOrPlaceholder(from),
                          pathOrPlaceholder(to),
                          ec.message());
            return false;
        }

        auto notifyAssetMove = [this, &ctx](const std::filesystem::path& old_rel,
                                            const std::filesystem::path& new_rel) {
            if (old_rel.empty() || new_rel.empty())
                return;
            if (old_rel.extension() == ".meta" || new_rel.extension() == ".meta")
                return;

            if (ctx.asset_actions.notify_asset_source_event)
            {
                AssetSourceEvent event{};
                event.type = AssetSourceEventType::Moved;
                event.old_path = relToAssetVPath(old_rel);
                event.new_path = relToAssetVPath(new_rel);
                ctx.asset_actions.notify_asset_source_event(event);
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
        {
            HBD_CORE_WARN("{} rename_failed from={} to={} reason={}",
                          kProjectPanelLogTag,
                          pathOrPlaceholder(from),
                          pathOrPlaceholder(to),
                          ec.message());
            return false;
        }

        if (from_is_dir)
        {
            auto old_rel = std::filesystem::relative(from, m_assetsRoot, ec);
            if (ec)
            {
                HBD_CORE_WARN("{} rename_failed from={} to={} reason={}",
                              kProjectPanelLogTag,
                              pathOrPlaceholder(from),
                              pathOrPlaceholder(to),
                              ec.message());
                return false;
            }

            ec.clear();
            auto new_rel = std::filesystem::relative(to, m_assetsRoot, ec);
            if (ec)
            {
                HBD_CORE_WARN("{} rename_failed from={} to={} reason={}",
                              kProjectPanelLogTag,
                              pathOrPlaceholder(from),
                              pathOrPlaceholder(to),
                              ec.message());
                return false;
            }

            if (ctx.asset_actions.request_rename_folder)
            {
                const bool rename_requested =
                    ctx.asset_actions.request_rename_folder(relToAssetVPath(old_rel), relToAssetVPath(new_rel));
                if (!rename_requested)
                {
                    HBD_CORE_WARN("{} rename_folder_request_failed from={} to={}",
                                  kProjectPanelLogTag,
                                  relToAssetVPath(old_rel),
                                  relToAssetVPath(new_rel));
                    return false;
                }

                HBD_CORE_INFO("{} rename_completed from={} to={} entry_type=directory",
                              kProjectPanelLogTag,
                              pathOrPlaceholder(from),
                              pathOrPlaceholder(to));
                return true;
            }

            for (const auto& old_file_rel : old_files)
            {
                std::filesystem::path old_abs = m_assetsRoot / old_file_rel;
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

                notifyAssetMove(old_file_rel, new_rel);
            }
            HBD_CORE_INFO("{} rename_completed from={} to={} entry_type=directory",
                          kProjectPanelLogTag,
                          pathOrPlaceholder(from),
                          pathOrPlaceholder(to));
            return true;
        }

        auto old_rel = std::filesystem::relative(from, m_assetsRoot, ec);
        if (ec)
        {
            HBD_CORE_WARN("{} rename_failed from={} to={} reason={}",
                          kProjectPanelLogTag,
                          pathOrPlaceholder(from),
                          pathOrPlaceholder(to),
                          ec.message());
            return false;
        }

        ec.clear();
        auto new_rel = std::filesystem::relative(to, m_assetsRoot, ec);
        if (ec)
        {
            HBD_CORE_WARN("{} rename_failed from={} to={} reason={}",
                          kProjectPanelLogTag,
                          pathOrPlaceholder(from),
                          pathOrPlaceholder(to),
                          ec.message());
            return false;
        }

        notifyAssetMove(old_rel, new_rel);
        HBD_CORE_INFO("{} rename_completed from={} to={} entry_type=file",
                      kProjectPanelLogTag,
                      pathOrPlaceholder(from),
                      pathOrPlaceholder(to));

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
            HBD_CORE_WARN("{} directory_list_failed path={} reason={}",
                          kProjectPanelLogTag,
                          pathOrPlaceholder(dir),
                          ec.message());
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

    std::filesystem::path ProjectPanel::findRelocatedDirectoryCandidate() const
    {
        if (m_assetsRoot.empty() || m_entries.empty())
            return {};

        std::vector<std::string> child_names;
        child_names.reserve(m_entries.size());
        for (const auto& entry : m_entries)
        {
            const std::string name = entry.physical.filename().string();
            if (!name.empty())
                child_names.push_back(name);
        }
        if (child_names.empty())
            return {};

        auto score_directory = [this, &child_names](const std::filesystem::path& dir) -> size_t
        {
            std::error_code ec;
            if (!std::filesystem::exists(dir, ec) || !std::filesystem::is_directory(dir, ec))
                return 0;

            size_t score = 0;
            for (const auto& child_name : child_names)
            {
                if (std::filesystem::exists(dir / child_name, ec) && !ec)
                    ++score;
                ec.clear();
            }
            return score;
        };

        std::filesystem::path best_candidate;
        size_t best_score = 0;
        bool ambiguous = false;

        const auto nearest_existing_parent = findNearestExistingDirectory();
        if (!nearest_existing_parent.empty() && nearest_existing_parent != m_currentDir)
        {
            std::error_code ec;
            for (std::filesystem::directory_iterator it(nearest_existing_parent, ec), end; it != end; it.increment(ec))
            {
                if (ec)
                {
                    ec.clear();
                    continue;
                }
                if (!it->is_directory())
                    continue;
                if (isHiddenFile(it->path()))
                    continue;

                const size_t score = score_directory(it->path());
                if (score == 0)
                    continue;
                if (score > best_score)
                {
                    best_score = score;
                    best_candidate = it->path();
                    ambiguous = false;
                }
                else if (score == best_score)
                {
                    ambiguous = true;
                }
            }
        }

        if (best_score > 0 && !ambiguous)
            return best_candidate;

        std::error_code ec;
        for (std::filesystem::recursive_directory_iterator it(m_assetsRoot, ec), end; it != end; it.increment(ec))
        {
            if (ec)
            {
                ec.clear();
                continue;
            }
            if (!it->is_directory())
                continue;
            if (isHiddenFile(it->path()))
                continue;

            const auto& candidate = it->path();
            if (candidate == m_currentDir)
                continue;

            const size_t score = score_directory(candidate);
            if (score == 0)
                continue;

            if (score > best_score)
            {
                best_score = score;
                best_candidate = candidate;
                ambiguous = false;
            }
            else if (score == best_score)
            {
                ambiguous = true;
            }
        }

        if (best_score > 0 && !ambiguous)
            return best_candidate;

        return {};
    }

    std::filesystem::path ProjectPanel::findNearestExistingDirectory() const
    {
        if (m_assetsRoot.empty())
            return {};

        std::error_code ec;
        std::filesystem::path candidate = m_currentDir.empty() ? m_assetsRoot : m_currentDir;
        const std::filesystem::path root = m_assetsRoot.lexically_normal();

        while (!candidate.empty())
        {
            const std::filesystem::path normalized = candidate.lexically_normal();
            if (std::filesystem::exists(normalized, ec) && std::filesystem::is_directory(normalized, ec))
                return normalized;
            ec.clear();

            if (normalized == root)
                break;

            const auto parent = normalized.parent_path();
            if (parent.empty() || parent == normalized)
                break;
            candidate = parent;
        }

        if (std::filesystem::exists(root, ec) && std::filesystem::is_directory(root, ec))
            return root;
        return {};
    }

    bool ProjectPanel::ensureCurrentDirAvailable(EditorContext& ctx)
    {
        if (m_assetsRoot.empty())
            return false;

        std::error_code ec;
        if (!m_currentDir.empty() && std::filesystem::exists(m_currentDir, ec) && std::filesystem::is_directory(m_currentDir, ec))
            return false;

        const auto relocated = findRelocatedDirectoryCandidate();
        if (!relocated.empty())
        {
            m_currentDir = relocated;
            gatherEntries(m_currentDir);
            m_selectedRelStr.clear();
            ctx.setStatusMessage("Current folder was moved externally. View relocated.");
            HBD_CORE_INFO("{} current_dir_relocated path={}",
                          kProjectPanelLogTag,
                          pathOrPlaceholder(m_currentDir));
            return true;
        }

        const auto fallback = findNearestExistingDirectory();
        if (!fallback.empty())
        {
            m_currentDir = fallback;
            gatherEntries(m_currentDir);
            m_selectedRelStr.clear();
            ctx.setStatusMessage("Current folder no longer exists. Returned to nearest valid folder.");
            HBD_CORE_INFO("{} current_dir_fallback path={}",
                          kProjectPanelLogTag,
                          pathOrPlaceholder(m_currentDir));
            return true;
        }

        m_currentDir = m_assetsRoot;
        gatherEntries(m_currentDir);
        m_selectedRelStr.clear();
        ctx.setStatusMessage("Current folder no longer exists. Returned to Assets.");
        HBD_CORE_INFO("{} current_dir_reset path={}", kProjectPanelLogTag, pathOrPlaceholder(m_currentDir));
        return true;
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

            if (!e.is_dir && !relStr.empty())
            {
                AssetID asset_id{};
                if (ctx.asset_actions.find_asset_by_vpath)
                    asset_id = ctx.asset_actions.find_asset_by_vpath(relToAssetVPath(e.rel));

                if (asset_id.value != 0)
                    EditorDragDrop::BeginDragAsset(asset_id);
                else
                    EditorDragDrop::BeginDragProjectPath(relStr.c_str());
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
        if (!m_state.open)
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
                    if (ensureCurrentDirAvailable(ctx))
                        m_lastAutoRefresh = now;
                    gatherEntries(m_currentDir);
                    m_lastAutoRefresh = now;
                }
            }
        }

        ImGui::Begin(getName(), &m_state.open);

        const float panel_width = ImGui::GetContentRegionAvail().x;
        const float panel_height = ImGui::GetContentRegionAvail().y;
        const float splitter_width = 6.0f;
        const float min_tree_width = 120.0f;
        const float min_content_width = 200.0f;
        const float max_tree_width = std::max(min_tree_width, panel_width - splitter_width - min_content_width);

        if (m_leftPaneWidth <= 0.0f)
            m_leftPaneWidth = panel_width * 0.25f;

        m_leftPaneWidth = std::clamp(m_leftPaneWidth, min_tree_width, max_tree_width);

        ImGui::BeginChild("##ProjectTree", ImVec2(m_leftPaneWidth, 0.0f), false);
        renderDirectoryTree(ctx);
        ImGui::EndChild();

        ImGui::SameLine(0.0f, 0.0f);

        ImGui::InvisibleButton("##ProjectSplitter", ImVec2(splitter_width, panel_height));
        if (ImGui::IsItemHovered() || ImGui::IsItemActive())
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        if (ImGui::IsItemActive())
            m_leftPaneWidth = std::clamp(m_leftPaneWidth + ImGui::GetIO().MouseDelta.x, min_tree_width, max_tree_width);

        const ImVec2 splitter_min = ImGui::GetItemRectMin();
        const ImVec2 splitter_max = ImGui::GetItemRectMax();
        ImU32 splitter_color = ImGui::GetColorU32(ImGuiCol_Separator);
        if (ImGui::IsItemHovered() || ImGui::IsItemActive())
            splitter_color = ImGui::GetColorU32(ImGuiCol_SeparatorHovered);
        ImGui::GetWindowDrawList()->AddLine(ImVec2(splitter_min.x, splitter_min.y),
                                            ImVec2(splitter_min.x, splitter_max.y),
                                            splitter_color,
                                            1.0f);

        ImGui::SameLine(0.0f, 0.0f);

        ImGui::BeginChild("##ProjectContent", ImVec2(0.0f, 0.0f), false);
        renderPathHeader();
        ImGui::Separator();
        renderContent(ctx);
        ImGui::EndChild();

        renderCreateFolderPopup();
        ImGui::End();
    }
} // namespace Hybrid


