#include "picking_pass.h"

namespace Hybrid
{
    void PickingPass::execute(RenderContext& context)
    {
        // Picking currently piggybacks on ForwardPass COLOR1 output.
        (void)context;
    }
} // namespace Hybrid
