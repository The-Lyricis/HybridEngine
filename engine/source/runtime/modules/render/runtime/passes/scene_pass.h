#pragma once

#include "runtime/modules/render/runtime/pipeline/render_context.h"

namespace Hybrid
{
    class ScenePass
    {
    public:
        void execute(RenderContext& context);
    };
} // namespace Hybrid
