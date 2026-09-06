#include <iostream>

#include <glad/gl.h>

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

    auto& request = engine.getRenderFrameRequest();
    request.views.clear();
    Hybrid::RenderViewRequest scene_view{};
    scene_view.id = 41;
    scene_view.name = "LifecycleScene";
    scene_view.size = {96.0f, 64.0f};
    scene_view.camera_source = Hybrid::RenderCameraSource::ExplicitMatrices;
    Hybrid::RenderViewRequest game_view{};
    game_view.id = 42;
    game_view.name = "LifecycleGame";
    game_view.size = {160.0f, 90.0f};
    game_view.camera_source = Hybrid::RenderCameraSource::ExplicitMatrices;
    request.views = {scene_view, game_view};
    engine.run(2);
    const auto& result = engine.getRenderFrameResult();
    if (result.views.size() != 2 || result.views[0].id != 41 || result.views[1].id != 42 ||
        result.views[0].color_texture == 0 || result.views[1].color_texture == 0)
    {
        std::cerr << "stable multi-view target allocation failed\n";
        return 1;
    }
    GLint scene_width = 0;
    GLint scene_height = 0;
    GLint game_width = 0;
    GLint game_height = 0;
    glGetTextureLevelParameteriv(result.views[0].color_texture, 0, GL_TEXTURE_WIDTH, &scene_width);
    glGetTextureLevelParameteriv(result.views[0].color_texture, 0, GL_TEXTURE_HEIGHT, &scene_height);
    glGetTextureLevelParameteriv(result.views[1].color_texture, 0, GL_TEXTURE_WIDTH, &game_width);
    glGetTextureLevelParameteriv(result.views[1].color_texture, 0, GL_TEXTURE_HEIGHT, &game_height);
    if (scene_width != 96 || scene_height != 64 || game_width != 160 || game_height != 90)
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
