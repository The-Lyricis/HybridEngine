#include <iostream>

#include "runtime/modules/scene/scene.h"
#include "runtime/modules/scene/components/directional_light_component.h"
#include "runtime/runtime/engine.h"

int main()
{
    Hybrid::HybridEngine engine;
    Hybrid::EngineConfig config{};
    config.project_path = HYBRID_TEST_PROJECT_PATH;
    config.window_visible = false;
    config.worker_count = 2;
    if (!engine.initialize(config))
    {
        std::cerr << "hidden OpenGL engine initialization failed\n";
        return 1;
    }

    auto scene = engine.getSceneManager().getActiveScene();
    if (!scene)
    {
        std::cerr << "active scene was not created\n";
        return 1;
    }
    scene->createEntity("LifecycleDirectionalLight").AddComponent<Hybrid::DirectionalLightComponent>();

    auto& request = engine.getRenderFrameRequest();
    request.views.clear();
    Hybrid::RenderViewRequest scene_view{};
    scene_view.id = 41;
    scene_view.name = "LifecycleScene";
    scene_view.size = {96.0f, 64.0f};
    scene_view.camera_source = Hybrid::RenderCameraSource::ExplicitMatrices;
    scene_view.flags = Hybrid::RenderFlags::Scene | Hybrid::RenderFlags::Shadow |
                       Hybrid::RenderFlags::SelectionHighlight | Hybrid::RenderFlags::PostProcess;
    scene_view.selection.selected_entities.push_back(1);
    scene_view.post_process.enabled = true;
    scene_view.post_process.enable_tone_mapping = true;
    scene_view.post_process.enable_gamma_correction = true;
    Hybrid::RenderViewRequest game_view{};
    game_view.id = 42;
    game_view.name = "LifecycleGame";
    game_view.size = {160.0f, 90.0f};
    game_view.camera_source = Hybrid::RenderCameraSource::ExplicitMatrices;
    game_view.flags = Hybrid::RenderFlags::Scene | Hybrid::RenderFlags::PostProcess;
    game_view.post_process.enabled = true;
    request.views = {scene_view, game_view};
    engine.run(2);
    const auto& result = engine.getRenderFrameResult();
    if (result.views.size() != 2 || result.views[0].id != 41 || result.views[1].id != 42 ||
        !result.views[0].color_texture || !result.views[1].color_texture)
    {
        std::cerr << "stable multi-view target allocation failed\n";
        return 1;
    }
    const auto scene_desc = engine.getRenderSystem().device().textureDesc(result.views[0].color_texture);
    const auto game_desc = engine.getRenderSystem().device().textureDesc(result.views[1].color_texture);
    if (!scene_desc || !game_desc ||
        scene_desc.value.width != 96 || scene_desc.value.height != 64 ||
        game_desc.value.width != 160 || game_desc.value.height != 90)
    {
        std::cerr << "multi-view target dimensions are incorrect\n";
        return 1;
    }
    engine.shutdown();
    engine.shutdown();
    if (engine.isInitialized())
    {
        std::cerr << "engine remained initialized after shutdown\n";
        return 1;
    }
    std::cout << "HybridRenderLifecycleTests: all checks passed\n";
    return 0;
}
