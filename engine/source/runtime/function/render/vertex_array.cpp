#include "vertex_array.h"
#include "renderer_api.h"
#include "opengl/opengl_vertex_array.h"

namespace Hybrid {

    std::shared_ptr<VertexArray> VertexArray::Create() {
        switch (RendererAPI::getAPI()) {
        case RendererAPI::API::OpenGL:
            return std::make_shared<GLVertexArray>();
        default:
            return nullptr;
        }
    }

} // namespace Hybrid
