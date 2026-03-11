#pragma once

#include "runtime/modules/render/runtime/render_context.h"

namespace Hybrid
{
    class ForwardPass
    {
    public:
        void execute(RenderContext& context);
    };
} // namespace Hybrid
