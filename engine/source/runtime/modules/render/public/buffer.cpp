#include "buffer.h"
#include "renderer_api.h"
#include "runtime/modules/render/backend/opengl/opengl_buffer.h"

namespace Hybrid {

    std::shared_ptr<VertexBuffer> VertexBuffer::Create(const void* data, uint32_t size) {
        switch (RendererAPI::getAPI()) {
        case RendererAPI::API::OpenGL:
            return std::make_shared<GLVertexBuffer>(data, size);
        default:
            return nullptr;
        }
    }

    std::shared_ptr<IndexBuffer> IndexBuffer::Create(const uint32_t* indices, uint32_t count) {
        switch (RendererAPI::getAPI()) {
        case RendererAPI::API::OpenGL:
            return std::make_shared<GLIndexBuffer>(indices, count);
        default:
            return nullptr;
        }
    }

    std::shared_ptr<UniformBuffer> UniformBuffer::Create(uint32_t size) {
        switch (RendererAPI::getAPI()) {
        case RendererAPI::API::OpenGL:
            return std::make_shared<GLUniformBuffer>(size);
        default:
            return nullptr;
        }
    }

} // namespace Hybrid


