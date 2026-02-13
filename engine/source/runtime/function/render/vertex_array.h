#pragma once
#include <memory>
#include <cstdint>
#include "buffer.h"

namespace Hybrid {

    // VertexArray: gl-style VAO abstraction combining VBO + IBO.
    class VertexArray {
    public:
        virtual ~VertexArray() = default;

        virtual void bind() const = 0;
        virtual void unbind() const = 0;

        // M1 约定：只支持 1 个 VBO + 1 个 IBO，布局在实现里写死。
        virtual void setVertexBuffer(std::shared_ptr<VertexBuffer> vb) = 0;
        virtual void setIndexBuffer(std::shared_ptr<IndexBuffer> ib) = 0;

        virtual uint32_t getIndexCount() const = 0;

        static std::shared_ptr<VertexArray> Create();
    };

} // namespace Hybrid
