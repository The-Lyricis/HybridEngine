#include "framebuffer.h"
#include "renderer_api.h"
#include "opengl/opengl_framebuffer.h"

namespace Hybrid {

    std::shared_ptr<Framebuffer> Framebuffer::Create(const FramebufferSpec& spec) {
        switch (RendererAPI::getAPI()) {
        case RendererAPI::API::OpenGL:
            return std::make_shared<GLFramebuffer>(spec);
        default:
            return nullptr;
        }
    }

} // namespace Hybrid
