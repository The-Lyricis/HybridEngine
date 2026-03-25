#pragma once

#include "runtime/modules/render/runtime/render_flags.h"
#include "runtime/modules/render/runtime/render_packet.h"

namespace Hybrid
{
    struct FrameViewData
    {
        RenderFrameData frame;
        RenderDirLightData mainDirectionalLight;
        RenderFlags flags = RenderFlags::None;
    };
} // namespace Hybrid
