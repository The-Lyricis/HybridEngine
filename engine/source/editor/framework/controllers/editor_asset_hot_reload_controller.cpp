#include "editor_asset_hot_reload_controller.h"

#include <algorithm>
#include <utility>
#include <vector>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "editor/core/context/editor_context.h"
#include "runtime/core/base/macro.h"
#include "runtime/modules/asset/asset_registry.h"
#include "runtime/modules/asset/asset_type.h"
#include "runtime/modules/asset/runtime_resource_system.h"
#include "runtime/modules/render/runtime/render_system.h"
#include "runtime/modules/window/window_system.h"

namespace Hybrid
{
    namespace
    {
        constexpr const char* kEditorAssetHotReloadLogTag = "[EditorAssetHotReloadController]";

        const char* fileWatcherChangeTypeName(FileWatcherChangeType type)
        {
            switch (type)
            {
            case FileWatcherChangeType::Added:
                return "added";
            case FileWatcherChangeType::Modified:
                return "modified";
            case FileWatcherChangeType::Removed:
                return "removed";
            default:
                return "unknown";
            }
        }

        const char* assetSourceChangeTypeName(AssetSourceChangeType type)
        {
            switch (type)
            {
            case AssetSourceChangeType::Added:
                return "added";
            case AssetSourceChangeType::Modified:
                return "modified";
            case AssetSourceChangeType::Removed:
                return "removed";
            default:
                return "unknown";
            }
        }

        std::string pathOrPlaceholder(const std::filesystem::path& path)
        {
            return path.empty() ? std::string("<empty>") : path.generic_string();
        }
    } // namespace

    EditorAssetHotReloadController::EditorAssetHotReloadController(EngineServices services)
        : m_services(std::move(services))
    {
    }

    void EditorAssetHotReloadController::initialize(const std::function<void(const std::string&)>& status_sink)
    {
        m_status_sink = status_sink;
        m_assets_root.clear();
        m_file_watcher_poll_elapsed = 0.0f;
        m_file_watcher_last_window_focused = true;

        if (m_services.resources && m_services.resources->getRegistry())
        {
            m_assets_root = m_services.resources->getRegistry()->getRoot();
            if (!m_assets_root.empty() && !m_file_watcher.initialize(m_assets_root, true))
            {
                HBD_CORE_WARN("{} watcher_init_failed assets_root={}",
                              kEditorAssetHotReloadLogTag,
                              pathOrPlaceholder(m_assets_root));
            }
        }

        if (m_services.editor_resources)
        {
            m_services.editor_resources->bootstrapImportOnce();
            m_services.editor_resources->setAssetsReloadedCallback(
                [this](const AssetsReloadedEvent& event) { handleAssetsReloaded(event); });
        }

        if (m_services.window)
        {
            if (GLFWwindow* window = m_services.window->getNativeWindow())
                m_file_watcher_last_window_focused = glfwGetWindowAttrib(window, GLFW_FOCUSED) != 0;
        }

        m_initialized = true;
        HBD_CORE_INFO("{} initialize_completed assets_root={} watcher_initialized={}",
                      kEditorAssetHotReloadLogTag,
                      pathOrPlaceholder(m_assets_root),
                      m_file_watcher.isInitialized());
    }

    void EditorAssetHotReloadController::shutdown()
    {
        if (!m_initialized)
            return;

        if (m_services.editor_resources)
            m_services.editor_resources->clearAssetsReloadedCallback();

        m_assets_root.clear();
        m_file_watcher_poll_elapsed = 0.0f;
        m_file_watcher_last_window_focused = true;
        m_status_sink = {};
        m_initialized = false;
        HBD_CORE_INFO("{} shutdown_completed", kEditorAssetHotReloadLogTag);
    }

    void EditorAssetHotReloadController::bindContext(EditorContext& ctx)
    {
        ctx.asset_actions.notify_asset_source_event = [this](const AssetSourceEvent& event) {
            if (!m_services.editor_resources)
                return;

            switch (event.type)
            {
            case AssetSourceEventType::Added:
                HBD_CORE_DEBUG("{} enqueue_requested path={} change=added",
                               kEditorAssetHotReloadLogTag,
                               event.path);
                m_services.editor_resources->enqueueSourceChanged(event.path, AssetSourceChangeType::Added);
                break;
            case AssetSourceEventType::Modified:
                HBD_CORE_DEBUG("{} enqueue_requested path={} change=modified",
                               kEditorAssetHotReloadLogTag,
                               event.path);
                m_services.editor_resources->enqueueSourceChanged(event.path, AssetSourceChangeType::Modified);
                break;
            case AssetSourceEventType::Removed:
                HBD_CORE_DEBUG("{} enqueue_requested path={} change=removed",
                               kEditorAssetHotReloadLogTag,
                               event.path);
                m_services.editor_resources->enqueueSourceChanged(event.path, AssetSourceChangeType::Removed);
                break;
            case AssetSourceEventType::Moved:
                HBD_CORE_DEBUG("{} move_requested old_path={} new_path={}",
                               kEditorAssetHotReloadLogTag,
                               event.old_path,
                               event.new_path);
                (void)m_services.editor_resources->moveAsset(event.old_path, event.new_path);
                break;
            default:
                break;
            }
        };
    }

    void EditorAssetHotReloadController::unbindContext(EditorContext& ctx)
    {
        ctx.asset_actions.notify_asset_source_event = {};
    }

    void EditorAssetHotReloadController::update(float dt)
    {
        if (!m_initialized)
            return;

        pollFileWatcher(dt);

        if (m_services.editor_resources)
            m_services.editor_resources->processImportQueue(4, 2);
    }

    bool EditorAssetHotReloadController::requestReimport(const std::string& asset_vpath)
    {
        if (asset_vpath.empty() || !m_services.editor_resources)
        {
            HBD_CORE_WARN("{} reimport_request_rejected path={} editor_resources_ready={}",
                          kEditorAssetHotReloadLogTag,
                          asset_vpath.empty() ? "<empty>" : asset_vpath,
                          m_services.editor_resources != nullptr);
            return false;
        }

        m_services.editor_resources->enqueueManualReimport(asset_vpath);
        setStatusMessage("Reimport queued.");
        HBD_CORE_INFO("{} reimport_requested path={}", kEditorAssetHotReloadLogTag, asset_vpath);
        return true;
    }

    bool EditorAssetHotReloadController::requestRenameFolder(const std::string& old_folder_vpath,
                                                             const std::string& new_folder_vpath)
    {
        if (old_folder_vpath.empty() || new_folder_vpath.empty() || !m_services.editor_resources)
        {
            HBD_CORE_WARN("{} rename_folder_request_rejected old_path={} new_path={} editor_resources_ready={}",
                          kEditorAssetHotReloadLogTag,
                          old_folder_vpath.empty() ? "<empty>" : old_folder_vpath,
                          new_folder_vpath.empty() ? "<empty>" : new_folder_vpath,
                          m_services.editor_resources != nullptr);
            return false;
        }

        if (!m_services.editor_resources->renameFolder(old_folder_vpath, new_folder_vpath))
        {
            HBD_CORE_WARN("{} rename_folder_request_failed old_path={} new_path={}",
                          kEditorAssetHotReloadLogTag,
                          old_folder_vpath,
                          new_folder_vpath);
            return false;
        }

        setStatusMessage("Folder renamed.");
        HBD_CORE_INFO("{} rename_folder_requested old_path={} new_path={}",
                      kEditorAssetHotReloadLogTag,
                      old_folder_vpath,
                      new_folder_vpath);
        return true;
    }

    void EditorAssetHotReloadController::handleAssetsReloaded(const AssetsReloadedEvent& event)
    {
        if (m_services.render)
        {
            for (const auto& meta : event.assets)
                m_services.render->invalidateAsset(meta.id, meta.type);
        }

        if (!event.assets.empty())
        {
            setStatusMessage("Assets reloaded.");
            HBD_CORE_INFO("{} assets_reloaded count={}", kEditorAssetHotReloadLogTag, event.assets.size());
        }
    }

    void EditorAssetHotReloadController::pollFileWatcher(float dt)
    {
        if (!m_services.editor_resources || !m_file_watcher.isInitialized())
            return;

        const bool window_focused = [this]() -> bool
        {
            if (!m_services.window)
                return true;

            GLFWwindow* window = m_services.window->getNativeWindow();
            if (!window)
                return true;

            return glfwGetWindowAttrib(window, GLFW_FOCUSED) != 0;
        }();

        if (window_focused && !m_file_watcher_last_window_focused)
            m_file_watcher_poll_elapsed = getFileWatcherPollInterval();

        m_file_watcher_last_window_focused = window_focused;
        m_file_watcher_poll_elapsed += std::max(0.0f, dt);
        if (m_file_watcher_poll_elapsed < getFileWatcherPollInterval())
            return;

        m_file_watcher_poll_elapsed = 0.0f;
        std::vector<std::pair<std::filesystem::path, FileWatcherChangeType>> events;
        m_file_watcher.poll([&events](const std::filesystem::path& physical_path, FileWatcherChangeType type) {
            events.emplace_back(physical_path, type);
        });

        bool has_topology_change = false;
        for (const auto& [path, type] : events)
        {
            (void)path;
            if (type == FileWatcherChangeType::Added || type == FileWatcherChangeType::Removed)
            {
                has_topology_change = true;
                break;
            }
        }

        if (has_topology_change)
        {
            HBD_CORE_DEBUG("{} reconcile_requested reason=topology_change event_count={}",
                           kEditorAssetHotReloadLogTag,
                           events.size());
            (void)m_services.editor_resources->reconcileMovedAssets();
        }

        auto registry = m_services.resources ? m_services.resources->getRegistry() : nullptr;
        for (const auto& [physical_path, type] : events)
        {
            std::string source_vpath;
            if (!toAssetVPath(physical_path, source_vpath))
            {
                HBD_CORE_DEBUG("{} change_ignored reason=outside_assets_root path={} watcher_change={}",
                               kEditorAssetHotReloadLogTag,
                               pathOrPlaceholder(physical_path),
                               fileWatcherChangeTypeName(type));
                continue;
            }

            if (registry)
            {
                if (type == FileWatcherChangeType::Added && registry->findByPath(source_vpath))
                {
                    HBD_CORE_DEBUG("{} change_ignored reason=asset_already_registered path={} watcher_change={}",
                                   kEditorAssetHotReloadLogTag,
                                   source_vpath,
                                   fileWatcherChangeTypeName(type));
                    continue;
                }
                if (type == FileWatcherChangeType::Removed && !registry->findByPath(source_vpath))
                {
                    HBD_CORE_DEBUG("{} change_ignored reason=asset_not_registered path={} watcher_change={}",
                                   kEditorAssetHotReloadLogTag,
                                   source_vpath,
                                   fileWatcherChangeTypeName(type));
                    continue;
                }
            }

            const AssetSourceChangeType change =
                (type == FileWatcherChangeType::Removed)
                    ? AssetSourceChangeType::Removed
                    : (type == FileWatcherChangeType::Added ? AssetSourceChangeType::Added
                                                            : AssetSourceChangeType::Modified);
            HBD_CORE_DEBUG("{} enqueue_requested path={} change={} watcher_change={}",
                           kEditorAssetHotReloadLogTag,
                           source_vpath,
                           assetSourceChangeTypeName(change),
                           fileWatcherChangeTypeName(type));
            m_services.editor_resources->enqueueSourceChanged(source_vpath, change);
        }
    }

    float EditorAssetHotReloadController::getFileWatcherPollInterval() const
    {
        return m_file_watcher_last_window_focused
            ? m_file_watcher_poll_interval_sec
            : m_file_watcher_poll_unfocused_interval_sec;
    }

    bool EditorAssetHotReloadController::toAssetVPath(const std::filesystem::path& physical_path, std::string& out_vpath) const
    {
        out_vpath.clear();
        if (m_assets_root.empty() || physical_path.empty())
            return false;

        std::error_code ec;
        auto rel = std::filesystem::relative(physical_path, m_assets_root, ec);
        if (ec || rel.empty())
            return false;

        std::string rel_str = rel.generic_string();
        while (!rel_str.empty() && (rel_str.front() == '/' || rel_str.front() == '\\'))
            rel_str.erase(rel_str.begin());
        if (rel_str.empty())
            return false;

        out_vpath = std::string("asset:") + rel_str;
        return true;
    }

    void EditorAssetHotReloadController::setStatusMessage(const std::string& message) const
    {
        if (m_status_sink)
            m_status_sink(message);
    }
} // namespace Hybrid
