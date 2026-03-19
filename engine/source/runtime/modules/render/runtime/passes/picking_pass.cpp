#include "picking_pass.h"

namespace Hybrid
{
    void PickingPass::execute(RenderContext& context)
    {
        // Picking currently piggybacks on ScenePass COLOR1 output.
        (void)context;
    }
} // namespace Hybrid
