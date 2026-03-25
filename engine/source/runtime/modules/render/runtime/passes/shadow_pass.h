#pragma once

#include "runtime/modules/render/runtime/pipeline/render_context.h"

namespace Hybrid
{
    class ShadowPass
    {
    public:
        void execute(RenderContext& context);
    };
} // namespace Hybrid
