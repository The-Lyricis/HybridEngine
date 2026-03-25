#pragma once

#include "runtime/modules/render/runtime/frame_view_data.h"
#include "runtime/modules/render/runtime/render_packet.h"
#include "runtime/modules/render/runtime/render_shadow_settings.h"

namespace Hybrid
{
    struct ShadowFrameBuildInput
    {
        const FrameViewData* view = nullptr;
        const DirectionalShadowSettings* settings = nullptr;
    };

    class ShadowFrameBuilder
    {
    public:
        void build(const ShadowFrameBuildInput& input, RenderShadowData& out_shadow) const;
    };
} // namespace Hybrid
