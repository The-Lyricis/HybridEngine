#include "editor_asset_hot_reload_controller.h"

#include <algorithm>
#include <utility>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "editor/core/editor_context.h"
#include "runtime/core/base/macro.h"
#include "runtime/modules/asset/asset_registry.h"
#include "runtime/modules/asset/asset_type.h"
#include "runtime/modules/asset/runtime_resource_system.h"
#include "runtime/modules/render/runtime/render_system.h"
#include "runtime/modules/window/window_system.h"

namespace Hybrid
{
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
                HBD_CORE_WARN("EditorAssetHotReloadController: file watcher init failed at {}", m_assets_root.string());
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
    }

    void EditorAssetHotReloadController::bindContext(EditorContext& ctx)
    {
        ctx.notify_asset_source_event = [this](const AssetSourceEvent& event) {
            if (!m_services.editor_resources)
                return;

            switch (event.type)
            {
            case AssetSourceEventType::Added:
                m_services.editor_resources->enqueueSourceChanged(event.path, AssetSourceChangeType::Added);
                break;
            case AssetSourceEventType::Modified:
                m_services.editor_resources->enqueueSourceChanged(event.path, AssetSourceChangeType::Modified);
                break;
            case AssetSourceEventType::Removed:
                m_services.editor_resources->enqueueSourceChanged(event.path, AssetSourceChangeType::Removed);
                break;
            case AssetSourceEventType::Moved:
                (void)m_services.editor_resources->moveAsset(event.old_path, event.new_path);
                break;
            default:
                break;
            }
        };
    }

    void EditorAssetHotReloadController::unbindContext(EditorContext& ctx)
    {
        ctx.notify_asset_source_event = {};
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
            return false;

        m_services.editor_resources->enqueueManualReimport(asset_vpath);
        setStatusMessage("Reimport queued.");
        return true;
    }

    void EditorAssetHotReloadController::handleAssetsReloaded(const AssetsReloadedEvent& event)
    {
        if (m_services.resources)
        {
            for (const auto& meta : event.assets)
                m_services.resources->invalidateAsset(meta.id);
        }

        if (m_services.render)
        {
            for (const auto& meta : event.assets)
                m_services.render->invalidateAsset(meta.id, meta.type);
        }

        if (!event.assets.empty())
            setStatusMessage("Assets reloaded.");
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
        m_file_watcher.poll([this](const std::filesystem::path& physical_path, FileWatcherChangeType type) {
            std::string source_vpath;
            if (!toAssetVPath(physical_path, source_vpath))
                return;

            const AssetSourceChangeType change =
                (type == FileWatcherChangeType::Removed)
                    ? AssetSourceChangeType::Removed
                    : (type == FileWatcherChangeType::Added ? AssetSourceChangeType::Added
                                                            : AssetSourceChangeType::Modified);
            m_services.editor_resources->enqueueSourceChanged(source_vpath, change);
        });
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
