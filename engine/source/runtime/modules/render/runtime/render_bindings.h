#pragma once

#include <cstdint>

namespace Hybrid::RenderBindings
{
    inline constexpr uint32_t kSceneAlbedoSlot = 0;
    inline constexpr uint32_t kSceneNormalSlot = 1;
    inline constexpr uint32_t kSceneMRSlot = 2;
    inline constexpr uint32_t kSceneAOSlot = 3;
    inline constexpr uint32_t kSceneEmissiveSlot = 4;

    inline constexpr const char* kSceneBaseColorTextureUniform = "u_BaseColorTexture";
    inline constexpr const char* kSceneNormalUniform = "u_NormalMap";
    inline constexpr const char* kSceneMetallicRoughnessTextureUniform = "u_MetallicRoughnessTexture";
    inline constexpr const char* kSceneOcclusionTextureUniform = "u_OcclusionTexture";
    inline constexpr const char* kSceneEmissiveTextureUniform = "u_EmissiveTexture";
    inline constexpr const char* kSceneBaseColorFactorUniform = "u_BaseColorFactor";
    inline constexpr const char* kSceneMetallicFactorUniform = "u_MetallicFactor";
    inline constexpr const char* kSceneRoughnessFactorUniform = "u_RoughnessFactor";
    inline constexpr const char* kSceneOcclusionStrengthUniform = "u_OcclusionStrength";
    inline constexpr const char* kSceneEmissiveFactorUniform = "u_EmissiveFactor";
    inline constexpr const char* kSceneHasNormalMapUniform = "u_HasNormalMap";
    inline constexpr const char* kSceneAlphaModeUniform = "u_AlphaMode";
    inline constexpr const char* kSceneAlphaCutoffUniform = "u_AlphaCutoff";
    inline constexpr const char* kSceneModelUniform = "u_Model";
    inline constexpr const char* kSceneTintColorUniform = "u_TintColor";
    inline constexpr const char* kSceneEntityIDUniform = "u_EntityID";

    inline constexpr uint32_t kSkyboxCubemapSlot = 5;
    inline constexpr const char* kSkyboxCubemapUniform = "u_SkyboxCubemap";

    inline constexpr uint32_t kSceneShadowMapSlot = 6;
    inline constexpr uint32_t kSceneShadowMapSlotCount = 4;
    inline constexpr const char* kSceneShadowMapUniform = "u_ShadowMaps";
    inline constexpr const char* kSceneShadowsEnabledUniform = "u_ShadowsEnabled";
    inline constexpr const char* kSceneShadowCascadeCountUniform = "u_ShadowCascadeCount";
    inline constexpr const char* kSceneLightViewProjectionsUniform = "u_LightViewProjections";
    inline constexpr const char* kSceneShadowCascadeSplitsUniform = "u_ShadowCascadeSplits";
    inline constexpr const char* kSceneShadowStrengthUniform = "u_ShadowStrength";
    inline constexpr const char* kSceneShadowBiasConstUniform = "u_ShadowBiasConst";
    inline constexpr const char* kSceneShadowBiasSlopeUniform = "u_ShadowBiasSlope";

    inline constexpr uint32_t kSelectionOverlaySceneColorSlot = 0;
    inline constexpr uint32_t kSelectionOverlaySceneDepthSlot = 1;
    inline constexpr uint32_t kSelectionOverlayMaskSlot = 2;
    inline constexpr uint32_t kSelectionOverlaySelectedDepthSlot = 3;

    inline constexpr const char* kSelectionOverlaySceneColorUniform = "u_SceneColorTex";
    inline constexpr const char* kSelectionOverlaySceneDepthUniform = "u_SceneDepthTex";
    inline constexpr const char* kSelectionOverlayMaskUniform = "u_SelectedMaskTex";
    inline constexpr const char* kSelectionOverlaySelectedDepthUniform = "u_SelectedDepthTex";
    inline constexpr const char* kSelectionOverlayTexelWidthUniform = "u_TexelWidth";
    inline constexpr const char* kSelectionOverlayTexelHeightUniform = "u_TexelHeight";
    inline constexpr const char* kSelectionOverlayVisibleColorUniform = "u_VisibleOutlineColor";
    inline constexpr const char* kSelectionOverlayOccludedColorUniform = "u_OccludedOutlineColor";
    inline constexpr const char* kSelectionOverlayFillColorUniform = "u_FillColor";
    inline constexpr const char* kSelectionOverlayDepthEpsilonUniform = "u_DepthEpsilon";

    inline constexpr uint32_t kPostProcessSceneColorSlot = 0;
    inline constexpr const char* kPostProcessSceneColorUniform = "u_SceneColorTex";
    inline constexpr const char* kPostProcessToneMappingUniform = "u_EnableToneMapping";
    inline constexpr const char* kPostProcessGammaCorrectionUniform = "u_EnableGammaCorrection";
    inline constexpr const char* kPostProcessExposureUniform = "u_Exposure";
    inline constexpr const char* kPostProcessGammaUniform = "u_Gamma";
} // namespace Hybrid::RenderBindings
