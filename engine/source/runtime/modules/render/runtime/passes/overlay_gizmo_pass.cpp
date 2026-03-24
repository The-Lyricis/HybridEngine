#include "overlay_gizmo_pass.h"

namespace Hybrid
{
    void OverlayGizmoPass::execute(RenderContext& context)
    {
        (void)context;
        // Overlay gizmos are intentionally split from world gizmos.
        // The actual manipulator/2D handle rendering path will live here.
    }
} // namespace Hybrid
