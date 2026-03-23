#pragma once

#include <cstdint>

namespace Hybrid::RenderBindings
{
    inline constexpr uint32_t kSceneAlbedoSlot = 0;
    inline constexpr uint32_t kSceneNormalSlot = 1;
    inline constexpr uint32_t kSceneMRSlot = 2;
    inline constexpr uint32_t kSceneAOSlot = 3;
    inline constexpr uint32_t kSceneEmissiveSlot = 4;

    inline constexpr const char* kSceneAlbedoUniform = "u_AlbedoMap";
    inline constexpr const char* kSceneNormalUniform = "u_NormalMap";
    inline constexpr const char* kSceneMRUniform = "u_MRMap";
    inline constexpr const char* kSceneAOUniform = "u_AOMap";
    inline constexpr const char* kSceneEmissiveUniform = "u_EmissiveMap";
    inline constexpr const char* kSceneAlbedoColorUniform = "u_AlbedoColor";
    inline constexpr const char* kSceneMetallicUniform = "u_Metallic";
    inline constexpr const char* kSceneRoughnessUniform = "u_Roughness";
    inline constexpr const char* kSceneAOScalarUniform = "u_AO";
    inline constexpr const char* kSceneEmissiveScalarUniform = "u_Emissive";
    inline constexpr const char* kSceneHasNormalMapUniform = "u_HasNormalMap";
    inline constexpr const char* kSceneSurfaceModeUniform = "u_SurfaceMode";
    inline constexpr const char* kSceneAlphaCutoffUniform = "u_AlphaCutoff";

    inline constexpr uint32_t kSkyboxCubemapSlot = 5;
    inline constexpr const char* kSkyboxCubemapUniform = "u_SkyboxCubemap";

    inline constexpr uint32_t kSelectionOverlaySceneColorSlot = 0;
    inline constexpr uint32_t kSelectionOverlaySceneDepthSlot = 1;
    inline constexpr uint32_t kSelectionOverlayMaskSlot = 2;
    inline constexpr uint32_t kSelectionOverlaySelectedDepthSlot = 3;

    inline constexpr const char* kSelectionOverlaySceneColorUniform = "u_SceneColorTex";
    inline constexpr const char* kSelectionOverlaySceneDepthUniform = "u_SceneDepthTex";
    inline constexpr const char* kSelectionOverlayMaskUniform = "u_SelectedMaskTex";
    inline constexpr const char* kSelectionOverlaySelectedDepthUniform = "u_SelectedDepthTex";
} // namespace Hybrid::RenderBindings
