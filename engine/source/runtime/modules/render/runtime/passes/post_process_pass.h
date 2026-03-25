#pragma once

#include "runtime/modules/render/runtime/pipeline/render_context.h"

namespace Hybrid
{
    class PostProcessPass
    {
    public:
        void execute(RenderContext& context);
    };
} // namespace Hybrid
