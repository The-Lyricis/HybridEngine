#include <iostream>
#include <memory>
#include <utility>

#include "runtime/engine.h"
#include "runtime/core/base/macro.h"
#include "editor/engine_services.h"
#include "editor/layers/editor_layer.h"
#include "editor/layers/imgui_layer.h"
#include "editor/function/asset/editor_resource_system.h"

int main(int argc, char** argv)
{
    Hybrid::HybridEngine engine;
    engine.initialize();

    auto editor_resources = std::make_shared<Hybrid::EditorResourceSystem>();
    if (!editor_resources->initialize(engine.getResourceSystem()))
    {
        HBD_CORE_ERROR("EditorResourceSystem initialize failed.");
        engine.shutdown();
        return 1;
    }

    Hybrid::EngineServices services{};
    services.window = &engine.getWindowSystem();
    services.render = &engine.getRenderSystem();
    services.scene = &engine.getSceneManager();
    services.resources = &engine.getResourceSystem();
    services.editor_resources = editor_resources.get();
    services.input = &engine.getInputLayer();
    services.frame_context = &engine.getFrameContext();
    services.render_flags = &engine.getRenderFlags();
    services.editor_ext = &engine.getEditorRenderExt();
    services.consume_pick_result = [&engine](uint32_t& id) { return engine.consumePickResult(id); };

    engine.pushOverlay(new Hybrid::ImGuiLayer(engine.getWindowSystem().getNativeWindow()));
    engine.pushLayer(new Hybrid::EditorLayer(std::move(services)));

    engine.run();
    engine.shutdown();
    return 0;
}
