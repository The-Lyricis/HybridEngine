#include "buffer.h"
#include "renderer_api.h"
#include "opengl/opengl_buffer.h"

namespace Hybrid {

    std::shared_ptr<VertexBuffer> VertexBuffer::Create(const void* data, uint32_t size) {
        switch (RendererAPI::getAPI()) {
        case RendererAPI::API::OpenGL:
            return std::make_shared<OpenGLVertexBuffer>(data, size);
        default:
            return nullptr;
        }
    }

    std::shared_ptr<IndexBuffer> IndexBuffer::Create(const uint32_t* indices, uint32_t count) {
        switch (RendererAPI::getAPI()) {
        case RendererAPI::API::OpenGL:
            return std::make_shared<OpenGLIndexBuffer>(indices, count);
        default:
            return nullptr;
        }
    }

} // namespace Hybrid
