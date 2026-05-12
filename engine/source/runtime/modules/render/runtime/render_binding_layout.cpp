#include "render_binding_layout.h"

#include <algorithm>
#include <string>

#include "runtime/modules/render/public/shader.h"
#include "runtime/modules/render/runtime/render_bindings.h"
#include "runtime/modules/render/runtime/render_uniforms.h"

namespace Hybrid
{
    namespace
    {
        bool IsTextureBinding(RenderBindingType type)
        {
            return type == RenderBindingType::Texture2D || type == RenderBindingType::TextureCube;
        }

        bool IsUniformBlockBinding(RenderBindingType type)
        {
            return type == RenderBindingType::UniformBlock;
        }
    } // namespace

    const RenderBindingLayoutDesc& GetSceneBindingLayout()
    {
        static const RenderBindingLayoutDesc layout{
            "Scene",
            {
                { RenderUniforms::kFrameBlockName, RenderBindingType::UniformBlock, RenderUniforms::kFrameUBOBinding },
                { RenderUniforms::kLightBlockName, RenderBindingType::UniformBlock, RenderUniforms::kLightUBOBinding },
                { RenderBindings::kSceneBaseColorTextureUniform, RenderBindingType::Texture2D, RenderBindings::kSceneAlbedoSlot },
                { RenderBindings::kSceneNormalUniform, RenderBindingType::Texture2D, RenderBindings::kSceneNormalSlot },
                { RenderBindings::kSceneMetallicRoughnessTextureUniform, RenderBindingType::Texture2D, RenderBindings::kSceneMRSlot },
                { RenderBindings::kSceneOcclusionTextureUniform, RenderBindingType::Texture2D, RenderBindings::kSceneAOSlot },
                { RenderBindings::kSceneEmissiveTextureUniform, RenderBindingType::Texture2D, RenderBindings::kSceneEmissiveSlot },
                {
                    RenderBindings::kSceneShadowMapUniform,
                    RenderBindingType::Texture2D,
                    RenderBindings::kSceneShadowMapSlot,
                    RenderBindings::kSceneShadowMapSlotCount,
                },
            },
        };
        return layout;
    }

    const RenderBindingLayoutDesc& GetSceneMaterialBindingLayout()
    {
        static const RenderBindingLayoutDesc layout{
            "SceneMaterial",
            {
                { RenderBindings::kSceneBaseColorTextureUniform, RenderBindingType::Texture2D, RenderBindings::kSceneAlbedoSlot },
                { RenderBindings::kSceneNormalUniform, RenderBindingType::Texture2D, RenderBindings::kSceneNormalSlot },
                { RenderBindings::kSceneMetallicRoughnessTextureUniform, RenderBindingType::Texture2D, RenderBindings::kSceneMRSlot },
                { RenderBindings::kSceneOcclusionTextureUniform, RenderBindingType::Texture2D, RenderBindings::kSceneAOSlot },
                { RenderBindings::kSceneEmissiveTextureUniform, RenderBindingType::Texture2D, RenderBindings::kSceneEmissiveSlot },
                { RenderBindings::kSceneBaseColorFactorUniform, RenderBindingType::UniformVec4 },
                { RenderBindings::kSceneMetallicFactorUniform, RenderBindingType::UniformFloat },
                { RenderBindings::kSceneRoughnessFactorUniform, RenderBindingType::UniformFloat },
                { RenderBindings::kSceneOcclusionStrengthUniform, RenderBindingType::UniformFloat },
                { RenderBindings::kSceneEmissiveFactorUniform, RenderBindingType::UniformVec3 },
                { RenderBindings::kSceneHasNormalMapUniform, RenderBindingType::UniformInt },
                { RenderBindings::kSceneAlphaModeUniform, RenderBindingType::UniformInt },
                { RenderBindings::kSceneAlphaCutoffUniform, RenderBindingType::UniformFloat },
            },
        };
        return layout;
    }

    const RenderBindingLayoutDesc& GetSceneDrawBindingLayout()
    {
        static const RenderBindingLayoutDesc layout{
            "SceneDraw",
            {
                { RenderBindings::kSceneModelUniform, RenderBindingType::UniformMat4 },
                { RenderBindings::kSceneTintColorUniform, RenderBindingType::UniformVec4 },
                { RenderBindings::kSceneEntityIDUniform, RenderBindingType::UniformUInt },
            },
        };
        return layout;
    }

    const RenderBindingLayoutDesc& GetSceneShadowBindingLayout()
    {
        static const RenderBindingLayoutDesc layout{
            "SceneShadow",
            {
                { RenderBindings::kSceneShadowsEnabledUniform, RenderBindingType::UniformInt },
                { RenderBindings::kSceneShadowCascadeCountUniform, RenderBindingType::UniformInt },
                {
                    RenderBindings::kSceneLightViewProjectionsUniform,
                    RenderBindingType::UniformMat4,
                    0,
                    RenderBindings::kSceneShadowMapSlotCount,
                },
                {
                    RenderBindings::kSceneShadowCascadeSplitsUniform,
                    RenderBindingType::UniformFloat,
                    0,
                    RenderBindings::kSceneShadowMapSlotCount,
                },
                { RenderBindings::kSceneShadowStrengthUniform, RenderBindingType::UniformFloat },
                { RenderBindings::kSceneShadowBiasConstUniform, RenderBindingType::UniformFloat },
                { RenderBindings::kSceneShadowBiasSlopeUniform, RenderBindingType::UniformFloat },
            },
        };
        return layout;
    }

    const RenderBindingLayoutDesc& GetSelectionMaskBindingLayout()
    {
        static const RenderBindingLayoutDesc layout{
            "SelectionMask",
            {
                { RenderUniforms::kFrameBlockName, RenderBindingType::UniformBlock, RenderUniforms::kFrameUBOBinding },
                { RenderBindings::kSceneBaseColorTextureUniform, RenderBindingType::Texture2D, RenderBindings::kSceneAlbedoSlot },
            },
        };
        return layout;
    }

    const RenderBindingLayoutDesc& GetSkyboxBindingLayout()
    {
        static const RenderBindingLayoutDesc layout{
            "Skybox",
            {
                { RenderUniforms::kFrameBlockName, RenderBindingType::UniformBlock, RenderUniforms::kFrameUBOBinding },
                { RenderBindings::kSkyboxCubemapUniform, RenderBindingType::TextureCube, RenderBindings::kSkyboxCubemapSlot },
            },
        };
        return layout;
    }

    const RenderBindingLayoutDesc& GetShadowDepthBindingLayout()
    {
        static const RenderBindingLayoutDesc layout{
            "ShadowDepth",
            {
                { RenderBindings::kSceneBaseColorTextureUniform, RenderBindingType::Texture2D, RenderBindings::kSceneAlbedoSlot },
            },
        };
        return layout;
    }

    const RenderBindingLayoutDesc& GetPostProcessBindingLayout()
    {
        static const RenderBindingLayoutDesc layout{
            "PostProcess",
            {
                {
                    RenderBindings::kPostProcessSceneColorUniform,
                    RenderBindingType::Texture2D,
                    RenderBindings::kPostProcessSceneColorSlot,
                },
                {
                    RenderBindings::kPostProcessToneMappingUniform,
                    RenderBindingType::UniformInt,
                    0,
                },
                {
                    RenderBindings::kPostProcessGammaCorrectionUniform,
                    RenderBindingType::UniformInt,
                    0,
                },
                {
                    RenderBindings::kPostProcessExposureUniform,
                    RenderBindingType::UniformFloat,
                    0,
                },
                {
                    RenderBindings::kPostProcessGammaUniform,
                    RenderBindingType::UniformFloat,
                    0,
                },
            },
        };
        return layout;
    }

    const RenderBindingLayoutDesc& GetSelectionOverlayBindingLayout()
    {
        static const RenderBindingLayoutDesc layout{
            "SelectionOverlay",
            {
                {
                    RenderBindings::kSelectionOverlaySceneColorUniform,
                    RenderBindingType::Texture2D,
                    RenderBindings::kSelectionOverlaySceneColorSlot,
                },
                {
                    RenderBindings::kSelectionOverlaySceneDepthUniform,
                    RenderBindingType::Texture2D,
                    RenderBindings::kSelectionOverlaySceneDepthSlot,
                },
                {
                    RenderBindings::kSelectionOverlayMaskUniform,
                    RenderBindingType::Texture2D,
                    RenderBindings::kSelectionOverlayMaskSlot,
                },
                {
                    RenderBindings::kSelectionOverlaySelectedDepthUniform,
                    RenderBindingType::Texture2D,
                    RenderBindings::kSelectionOverlaySelectedDepthSlot,
                },
                {
                    RenderBindings::kSelectionOverlayTexelWidthUniform,
                    RenderBindingType::UniformFloat,
                    0,
                },
                {
                    RenderBindings::kSelectionOverlayTexelHeightUniform,
                    RenderBindingType::UniformFloat,
                    0,
                },
                {
                    RenderBindings::kSelectionOverlayVisibleColorUniform,
                    RenderBindingType::UniformVec4,
                    0,
                },
                {
                    RenderBindings::kSelectionOverlayOccludedColorUniform,
                    RenderBindingType::UniformVec4,
                    0,
                },
                {
                    RenderBindings::kSelectionOverlayFillColorUniform,
                    RenderBindingType::UniformVec4,
                    0,
                },
                {
                    RenderBindings::kSelectionOverlayDepthEpsilonUniform,
                    RenderBindingType::UniformFloat,
                    0,
                },
            },
        };
        return layout;
    }

    const RenderBindingDesc* FindRenderBinding(const RenderBindingLayoutDesc& layout, std::string_view name)
    {
        for (const RenderBindingDesc& binding : layout.bindings)
        {
            if (binding.name == name)
                return &binding;
        }
        return nullptr;
    }

    void ApplyStaticTextureBindings(Shader& shader, const RenderBindingLayoutDesc& layout)
    {
        for (const RenderBindingDesc& binding : layout.bindings)
        {
            if (!IsTextureBinding(binding.type))
                continue;

            const uint32_t array_count = std::max(1u, binding.array_count);
            if (array_count == 1)
            {
                shader.setInt(std::string(binding.name), static_cast<int>(binding.slot));
                continue;
            }

            for (uint32_t array_index = 0; array_index < array_count; ++array_index)
            {
                shader.setInt(std::string(binding.name) + "[" + std::to_string(array_index) + "]",
                              static_cast<int>(binding.slot + array_index));
            }
        }
    }

    void ApplyStaticUniformBlockBindings(Shader& shader, const RenderBindingLayoutDesc& layout)
    {
        for (const RenderBindingDesc& binding : layout.bindings)
        {
            if (!IsUniformBlockBinding(binding.type))
                continue;

            shader.setUniformBlockBinding(std::string(binding.name), binding.slot);
        }
    }
} // namespace Hybrid
