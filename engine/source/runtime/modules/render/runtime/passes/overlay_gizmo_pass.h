#pragma once

#include "runtime/modules/render/runtime/pipeline/render_context.h"

namespace Hybrid
{
    class OverlayGizmoPass
    {
    public:
        void execute(RenderContext& context);
    };
} // namespace Hybrid
