#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

namespace Hybrid
{
    class Shader;

    enum class RenderBindingType : unsigned char
    {
        Texture2D,
        TextureCube,
        UniformInt,
        UniformUInt,
        UniformFloat,
        UniformVec3,
        UniformVec4,
        UniformMat4,
        UniformBlock,
    };

    struct RenderBindingDesc
    {
        std::string_view name;
        RenderBindingType type = RenderBindingType::UniformFloat;
        uint32_t slot = 0;
        uint32_t array_count = 1;
    };

    struct RenderBindingLayoutDesc
    {
        std::string_view name;
        std::vector<RenderBindingDesc> bindings;
    };

    const RenderBindingLayoutDesc& GetSceneBindingLayout();
    const RenderBindingLayoutDesc& GetSceneMaterialBindingLayout();
    const RenderBindingLayoutDesc& GetSceneDrawBindingLayout();
    const RenderBindingLayoutDesc& GetSceneShadowBindingLayout();
    const RenderBindingLayoutDesc& GetSelectionMaskBindingLayout();
    const RenderBindingLayoutDesc& GetSkyboxBindingLayout();
    const RenderBindingLayoutDesc& GetShadowDepthBindingLayout();
    const RenderBindingLayoutDesc& GetPostProcessBindingLayout();
    const RenderBindingLayoutDesc& GetSelectionOverlayBindingLayout();
    const RenderBindingDesc* FindRenderBinding(const RenderBindingLayoutDesc& layout, std::string_view name);
    void ApplyStaticTextureBindings(Shader& shader, const RenderBindingLayoutDesc& layout);
    void ApplyStaticUniformBlockBindings(Shader& shader, const RenderBindingLayoutDesc& layout);
} // namespace Hybrid
