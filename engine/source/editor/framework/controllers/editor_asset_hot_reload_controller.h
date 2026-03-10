#pragma once

#include <filesystem>
#include <functional>
#include <string>

#include "editor/core/engine_services.h"
#include "editor/services/asset/editor_resource_system.h"
#include "editor/services/asset/file_watcher.h"

namespace Hybrid
{
    struct EditorContext;

    class EditorAssetHotReloadController
    {
    public:
        explicit EditorAssetHotReloadController(EngineServices services = {});

        void initialize(const std::function<void(const std::string&)>& status_sink = {});
        void shutdown();

        void bindContext(EditorContext& ctx);
        void unbindContext(EditorContext& ctx);

        void update(float dt);
        bool requestReimport(const std::string& asset_vpath);

    private:
        void handleAssetsReloaded(const AssetsReloadedEvent& event);
        void pollFileWatcher(float dt);
        float getFileWatcherPollInterval() const;
        bool toAssetVPath(const std::filesystem::path& physical_path, std::string& out_vpath) const;
        void setStatusMessage(const std::string& message) const;

    private:
        EngineServices m_services{};
        PollingFileWatcher m_file_watcher;
        std::filesystem::path m_assets_root;
        std::function<void(const std::string&)> m_status_sink;
        float m_file_watcher_poll_elapsed = 0.0f;
        float m_file_watcher_poll_interval_sec = 0.5f;
        float m_file_watcher_poll_unfocused_interval_sec = 2.0f;
        bool m_file_watcher_last_window_focused = true;
        bool m_initialized = false;
    };
} // namespace Hybrid
