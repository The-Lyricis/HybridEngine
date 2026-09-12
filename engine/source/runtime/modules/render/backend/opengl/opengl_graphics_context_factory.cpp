#include "runtime/modules/render/public/graphics_context.h"

#include "runtime/modules/render/backend/opengl/opengl_context.h"
#include "runtime/modules/render/public/renderer_api.h"

namespace Hybrid
{
    std::unique_ptr<GraphicsContext> GraphicsContext::Create(IWindow& window)
    {
        if (RendererAPI::getAPI() == RendererAPI::API::OpenGL)
            return std::make_unique<GLContext>(window);
        return nullptr;
    }
} // namespace Hybrid
