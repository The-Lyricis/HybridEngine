#pragma once

#include <memory>

#include "runtime/modules/render/rhi/render_device.h"

namespace Hybrid
{
    class IWindow;

    struct RenderBackendBundle
    {
        std::unique_ptr<IRenderDevice> device;
        std::unique_ptr<ISwapchain> swapchain;
    };

    RhiResult<RenderBackendBundle> CreateRenderBackend(const RenderDeviceDesc& desc,
                                                       IWindow& window);
} // namespace Hybrid
