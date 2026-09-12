#include "runtime/modules/render/rhi/render_backend_factory.h"

#include "runtime/core/platform/window.h"

namespace Hybrid
{
    RhiResult<RenderBackendBundle> CreateRenderBackend(const RenderDeviceDesc& desc,
                                                       IWindow& window)
    {
        if (window.graphicsBackend() != desc.backend)
        {
            return RhiResult<RenderBackendBundle>::Failure(
                RhiErrorCode::InvalidArgument,
                "window graphics backend does not match render device request");
        }

        switch (desc.backend)
        {
        case GraphicsBackend::OpenGL:
        {
            RenderBackendBundle bundle{};
            bundle.device = CreateOpenGLRenderDevice(desc.validation_enabled);
            bundle.swapchain = CreateOpenGLSwapchain(window);
            if (!bundle.device || !bundle.swapchain)
            {
                return RhiResult<RenderBackendBundle>::Failure(
                    RhiErrorCode::ResourceCreationFailed,
                    "OpenGL render device or swapchain creation failed");
            }
            return RhiResult<RenderBackendBundle>::Success(std::move(bundle));
        }
        case GraphicsBackend::Null:
        case GraphicsBackend::Metal:
        case GraphicsBackend::Vulkan:
            return RhiResult<RenderBackendBundle>::Failure(
                RhiErrorCode::UnsupportedBackend,
                std::string("render backend is not linked: ") + ToString(desc.backend));
        }

        return RhiResult<RenderBackendBundle>::Failure(
            RhiErrorCode::UnsupportedBackend,
            "unknown render backend");
    }
} // namespace Hybrid
