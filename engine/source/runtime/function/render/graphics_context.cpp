#include "graphics_context.h"
#include "renderer_api.h"
#include "opengl/opengl_context.h"
#include <GLFW/glfw3.h>

namespace Hybrid {

    std::unique_ptr<GraphicsContext> GraphicsContext::Create(void* nativeWindow) {
        switch (RendererAPI::getAPI()) {
        case RendererAPI::API::OpenGL:
            return std::make_unique<GLContext>(static_cast<GLFWwindow*>(nativeWindow));
        default:
            return nullptr;
        }
    }

} // namespace Hybrid
