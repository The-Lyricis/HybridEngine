#pragma once
#include "runtime/modules/render/public/vertex_array.h"

namespace Hybrid {

    // GLVertexArray: manages VAO binding/state for OpenGL backend.
    class GLVertexArray final : public VertexArray {
    public:
        GLVertexArray();
        ~GLVertexArray() override;

        void bind() const override;
        void unbind() const override;

        void setVertexBuffer(std::shared_ptr<VertexBuffer> vb, const VertexLayout& layout) override;
        void setIndexBuffer(std::shared_ptr<IndexBuffer> ib) override;

        uint32_t getIndexCount() const override;

    private:
        uint32_t m_RendererID = 0;
        std::shared_ptr<VertexBuffer> m_VertexBuffer;
        std::shared_ptr<IndexBuffer>  m_IndexBuffer;
    };

} // namespace Hybrid


