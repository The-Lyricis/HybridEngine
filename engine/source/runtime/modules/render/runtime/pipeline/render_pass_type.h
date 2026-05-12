#pragma once

namespace Hybrid
{
    enum class RenderPassType : unsigned char
    {
        Scene,
        Skybox,
        Picking,
        SelectionMask,
        SelectionOverlay,
        WorldGizmo,
        OverlayGizmo,
        Grid,
        Shadow,
        PostProcess,
        //DebugNormals
    };
} // namespace Hybrid
