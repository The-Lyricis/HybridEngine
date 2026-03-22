#include "editor_app.h"

#include <memory>
#include <utility>

#include "editor/core/engine_services.h"
#include "editor/framework/layers/editor_layer.h"
#include "editor/framework/layers/imgui_layer.h"
#include "editor/platform/windows/editor_platform_services_win32.h"
#include "editor/services/asset/editor_resource_system.h"
#include "runtime/core/base/macro.h"
#include "runtime/runtime/engine.h"

namespace Hybrid
{
    int EditorApp::run(int argc, char** argv)
    {
        constexpr const char* kEditorAppLogTag = "[EditorApp]";
        (void)argc;
        (void)argv;

        HybridEngine engine;
        engine.initialize();
        HBD_CORE_INFO("{} engine_initialized", kEditorAppLogTag);
        HBD_CORE_INFO("{} run_started", kEditorAppLogTag);

        auto editor_resources = std::make_shared<EditorResourceSystem>();
        auto platform_services = std::make_unique<EditorPlatformServicesWin32>();
        if (!editor_resources->initialize(engine.getResourceSystem()))
        {
            HBD_CORE_ERROR("{} editor_resources_initialize_failed", kEditorAppLogTag);
            engine.shutdown();
            return 1;
        }

        EngineServices services{};
        services.window = &engine.getWindowSystem();
        services.render = &engine.getRenderSystem();
        services.scene = &engine.getSceneManager();
        services.resources = &engine.getResourceSystem();
        services.editor_resources = editor_resources.get();
        services.platform = platform_services.get();
        services.input = &engine.getInputLayer();
        services.frame_context = &engine.getFrameContext();
        services.render_flags = &engine.getRenderFlags();
        services.editor_ext = &engine.getEditorRenderExt();
        services.consume_pick_result = [&engine](uint32_t& id) { return engine.consumePickResult(id); };
        services.set_editor_scene = [&engine](std::shared_ptr<Scene> scene) -> bool
            {
                return engine.setEditorScene(std::move(scene));
            };

        engine.pushOverlay(new ImGuiLayer(engine.getWindowSystem().getNativeWindow()));
        auto* editor_layer = new EditorLayer(std::move(services));

        EditorModeCallbacks mode_callbacks;
        mode_callbacks.enter_play_mode_from_scene = [&engine](std::shared_ptr<Scene> scene) -> bool
            {
                return engine.enterPlayModeFromScene(scene);
            };
        mode_callbacks.exit_play_mode = [&engine]()
            {
                engine.exitPlayMode();
            };
        mode_callbacks.toggle_pause_mode = [&engine]()
            {
                engine.togglePlayPause();
            };
        mode_callbacks.is_play_mode = [&engine]() -> bool
            {
                return engine.isPlayMode();
            };
        mode_callbacks.is_pause_mode = [&engine]() -> bool
            {
                return engine.isPlayPaused();
            };
        editor_layer->setModeCallbacks(std::move(mode_callbacks));

        engine.pushLayer(editor_layer);
        HBD_CORE_INFO("{} layers_attached", kEditorAppLogTag);

        engine.run();
        HBD_CORE_INFO("{} run_completed exit_code=0", kEditorAppLogTag);
        engine.shutdown();
        return 0;
    }
} // namespace Hybrid

