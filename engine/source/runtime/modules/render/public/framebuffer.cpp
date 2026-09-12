#include "framebuffer.h"

#include <exception>

#include "runtime/modules/render/backend/opengl/opengl_framebuffer.h"
#include "runtime/modules/render/rhi/render_device.h"

namespace Hybrid {

    RhiResult<std::shared_ptr<Framebuffer>> Framebuffer::Create(const FramebufferSpec& spec,
                                                               IRenderDevice& device) {
        try
        {
            switch (device.backend())
            {
            case GraphicsBackend::OpenGL:
                return RhiResult<std::shared_ptr<Framebuffer>>::Success(
                    std::make_shared<GLFramebuffer>(spec, device));
            default:
                return RhiResult<std::shared_ptr<Framebuffer>>::Failure(
                    RhiErrorCode::UnsupportedBackend,
                    "legacy framebuffer adapter is not implemented for the selected render backend");
            }
        }
        catch (const std::exception& error)
        {
            return RhiResult<std::shared_ptr<Framebuffer>>::Failure(
                RhiErrorCode::ResourceCreationFailed, error.what());
        }
    }

} // namespace Hybrid

