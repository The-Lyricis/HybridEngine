#pragma once

#include <array>
#include <string_view>

namespace Hybrid::RenderShaders
{
    struct ShaderSourceDesc
    {
        std::string_view name;
        std::string_view vertex;
        std::string_view fragment;
    };

    inline constexpr ShaderSourceDesc kScene{
        "Scene",
        "Scene.vert",
        "Scene.frag"
    };

    inline constexpr ShaderSourceDesc kColliderDebug{
        "ColliderDebug",
        "ColliderDebug.vert",
        "ColliderDebug.frag"
    };

    inline constexpr ShaderSourceDesc kSkybox{
        "Skybox",
        "Skybox.vert",
        "Skybox.frag"
    };

    inline constexpr ShaderSourceDesc kSelectionMask{
        "SelectionMask",
        "SelectionMask.vert",
        "SelectionMask.frag"
    };

    inline constexpr ShaderSourceDesc kSelectionOverlay{
        "SelectionOverlay",
        "SelectionOverlay.vert",
        "SelectionOverlay.frag"
    };

    inline constexpr std::array<ShaderSourceDesc, 5> kBuiltinShaders = {
        kScene,
        kColliderDebug,
        kSkybox,
        kSelectionMask,
        kSelectionOverlay
    };
} // namespace Hybrid::RenderShaders
