#pragma once

#include <filesystem>
#include <functional>
#include <memory>

#include "editor/framework/camera/editor_camera.h"
#include "editor/framework/ui/editor_ui.h"
#include "editor/core/engine_services.h"
#include "editor/services/asset/file_watcher.h"
#include "editor/services/scene/editor_scene_io_service.h"
#include "runtime/core/event/layer.h"

namespace Hybrid
{
    struct EditorModeCallbacks
    {
        std::function<bool(std::shared_ptr<Scene>)> enter_play_mode_from_scene;
        std::function<void()> exit_play_mode;
        std::function<void()> toggle_pause_mode;
        std::function<bool()> is_play_mode;
        std::function<bool()> is_pause_mode;
    };
    // Editor orchestration layer: UI draw + bridge from EditorContext to render inputs.
    class EditorLayer final : public Layer
    {
    public:
        explicit EditorLayer(EngineServices services);

        void onAttach() override;          // Initialize editor UI and bind scene.
        void onDetach() override;          // Release editor UI resources.
        void onUpdate(float dt) override;  // Sync viewport state, camera input and render ext.
        void onImGuiRender() override;     // Draw panels and viewport.

        void setModeCallbacks(EditorModeCallbacks callbacks);

    private:
        void syncContextDocumentState();
        void updateFrameContext();         // Push current editor state into FrameContext/Flags/Ext.
        void updateEditorCamera(float dt);
        void bindAssetChangeCallback();
        void pollFileWatcher();
        bool toAssetVPath(const std::filesystem::path& physical_path, std::string& out_vpath) const;

    private:
        EngineServices m_services{};       // Injected runtime/editor services.
        EditorUI m_editor_ui;              // Panel/UI owner.
        EditorCamera m_editor_camera;      // Editor-only viewport camera.
        PollingFileWatcher m_file_watcher; // Editor-side polling watcher for Assets/.
        EditorSceneIOService m_scene_io;
        std::filesystem::path m_assets_root;
        bool m_initialized = false;        // Guard against partial startup/shutdown.
        EditorModeCallbacks m_mode_callbacks{};
    };
} // namespace Hybrid

