#include <iostream>
#include <vector>

#include "runtime/modules/asset/builtin_assets.h"
#include "runtime/modules/asset/asset_manager.h"
#include "runtime/modules/asset/asset_registry.h"
#include "runtime/modules/asset/material.h"
#include "runtime/modules/asset/runtime_resource_system.h"
#include "runtime/modules/asset/texture_image.h"
#include "runtime/modules/render/rhi/render_device.h"
#include "runtime/modules/scene/scene.h"
#include "runtime/modules/scene/components/directional_light_component.h"
#include "runtime/modules/scene/components/mesh_renderer_component.h"
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
    auto light_entity = scene->createEntity("LifecycleDirectionalLight");
    light_entity.AddComponent<Hybrid::DirectionalLightComponent>();
    const Hybrid::AssetID cube_id = engine.getResourceSystem().getBuiltinMeshID(Hybrid::BuiltinMesh::Cube);
    if (!cube_id.value)
    {
        std::cerr << "builtin cube mesh was not registered\n";
        return 1;
    }
    auto cube = scene->createEntity("LifecycleCube");
    auto registry = engine.getResourceSystem().getRegistry();
    auto assets = engine.getResourceSystem().getManager();
    const Hybrid::AssetID texture_id = registry->generateUniqueID();
    Hybrid::AssetMetadata texture_meta{};
    texture_meta.id = texture_id;
    texture_meta.type = Hybrid::AssetType::Texture2D;
    texture_meta.source_path = "builtin:LifecycleRedTexture";
    texture_meta.is_valid = true;
    registry->registerAsset(texture_meta);
    auto image = std::make_shared<Hybrid::TextureImageData>();
    image->width = 1;
    image->height = 1;
    image->pixels = {255, 0, 0, 255};
    assets->registerResident(texture_id, image);

    const Hybrid::AssetID material_id = registry->generateUniqueID();
    Hybrid::AssetMetadata material_meta{};
    material_meta.id = material_id;
    material_meta.type = Hybrid::AssetType::Material;
    material_meta.source_path = "builtin:LifecycleRedMaterial";
    material_meta.is_valid = true;
    registry->registerAsset(material_meta);
    Hybrid::MaterialData material_data{};
    material_data.alpha_mode = Hybrid::MaterialAlphaMode::Mask;
    material_data.base_color_texture.texture = texture_id;
    assets->registerResident(material_id, std::make_shared<Hybrid::Material>(material_data));
    auto& cube_renderer = cube.AddComponent<Hybrid::MeshRendererComponent>();
    cube_renderer.Mesh = cube_id;
    cube_renderer.Material = material_id;

    auto& request = engine.getRenderFrameRequest();
    request.views.clear();
    Hybrid::RenderViewRequest scene_view{};
    scene_view.id = 41;
    scene_view.name = "LifecycleScene";
    scene_view.size = {96.0f, 64.0f};
    scene_view.camera_source = Hybrid::RenderCameraSource::ExplicitMatrices;
    scene_view.flags = Hybrid::RenderFlags::Scene | Hybrid::RenderFlags::Shadow |
                       Hybrid::RenderFlags::SelectionHighlight | Hybrid::RenderFlags::PostProcess;
    scene_view.selection.selected_entities.push_back(cube.ToUInt());
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
    if (engine.getRenderSystem().getStats().submitted_draw_calls == 0)
    {
        std::cerr << "RHI scene mesh draw was not submitted\n";
        return 1;
    }
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
    std::vector<uint8_t> scene_pixels(96u * 64u * 4u);
    auto readback = engine.getRenderSystem().device().createCommandList();
    if (!readback || !readback->begin() ||
        !readback->readbackTexture(result.views[0].color_texture, scene_pixels.data(), scene_pixels.size()) ||
        !readback->end() || !engine.getRenderSystem().device().submit(*readback))
    {
        std::cerr << "RHI scene color readback failed\n";
        return 1;
    }
    bool has_drawn_pixels = false;
    bool has_red_material_pixel = false;
    for (size_t i = 4; i < scene_pixels.size(); i += 4)
    {
        if (scene_pixels[i] != scene_pixels[0] || scene_pixels[i + 1] != scene_pixels[1] ||
            scene_pixels[i + 2] != scene_pixels[2])
        {
            has_drawn_pixels = true;
        }
        if (scene_pixels[i] > scene_pixels[i + 1] + 20 &&
            scene_pixels[i] > scene_pixels[i + 2] + 20)
            has_red_material_pixel = true;
    }
    if (!has_drawn_pixels)
    {
        std::cerr << "RHI scene image contains only clear color\n";
        return 1;
    }
    if (!has_red_material_pixel)
    {
        std::cerr << "RHI material texture was not visible in the scene image\n";
        return 1;
    }
    light_entity.GetComponent<Hybrid::DirectionalLightComponent>().Intensity = 0.0f;
    request.scene = scene;
    const auto unlit_result = engine.getRenderSystem().renderFrame(request);
    if (unlit_result.views.empty() || !unlit_result.views[0].color_texture)
    {
        std::cerr << "unlit RHI scene render failed\n";
        return 1;
    }
    std::vector<uint8_t> unlit_pixels(scene_pixels.size());
    auto unlit_readback = engine.getRenderSystem().device().createCommandList();
    if (!unlit_readback || !unlit_readback->begin() ||
        !unlit_readback->readbackTexture(unlit_result.views[0].color_texture,
                                         unlit_pixels.data(), unlit_pixels.size()) ||
        !unlit_readback->end() || !engine.getRenderSystem().device().submit(*unlit_readback))
    {
        std::cerr << "unlit RHI scene readback failed\n";
        return 1;
    }
    bool lighting_changed_pixels = false;
    for (size_t i = 0; i < scene_pixels.size(); i += 4)
    {
        if (scene_pixels[i] > unlit_pixels[i] + 5)
        {
            lighting_changed_pixels = true;
            break;
        }
    }
    if (!lighting_changed_pixels)
    {
        std::cerr << "directional LightBlock did not affect the RHI scene image\n";
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
