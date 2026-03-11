#pragma once

#include "runtime/modules/render/runtime/render_context.h"

namespace Hybrid
{
    class PickingPass
    {
    public:
        void execute(RenderContext& context);
    };
} // namespace Hybrid
