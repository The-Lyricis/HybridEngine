#include "runtime/modules/render/backend/opengl/opengl_renderer_api.h"

namespace Hybrid {

    // For now we only support OpenGL; keep API as static state for factories.
    static RendererAPI::API s_CurrentAPI = RendererAPI::API::OpenGL;

    RendererAPI::API RendererAPI::getAPI() {
        return s_CurrentAPI;
    }

    std::unique_ptr<RendererAPI> RendererAPI::Create() {
        switch (s_CurrentAPI) {
        case API::OpenGL: return std::make_unique<GLRendererAPI>();
        default: return nullptr;
        }
    }

} // namespace Hybrid


