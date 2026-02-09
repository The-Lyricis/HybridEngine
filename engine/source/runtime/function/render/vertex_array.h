#pragma once
#include <memory>
#include "buffer.h"

namespace Hybrid {

    class VertexArray {
    public:
        VertexArray();
        ~VertexArray();

        void Bind() const;
        void Unbind() const;

        // M1 简化：只支持一个 VBO + 一个 IBO，且布局写死
        void SetVertexBuffer(std::shared_ptr<VertexBuffer> vb);
        void SetIndexBuffer(std::shared_ptr<IndexBuffer> ib);

        uint32_t GetIndexCount() const;

    private:
        uint32_t m_RendererID = 0;
        std::shared_ptr<VertexBuffer> m_VertexBuffer;
        std::shared_ptr<IndexBuffer>  m_IndexBuffer;
    };

} // namespace Hybrid
