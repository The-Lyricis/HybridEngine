#pragma once
#include <memory>
#include <cstdint>
#include <vector>
#include "buffer.h"

namespace Hybrid {

    struct VertexAttribute {
        uint32_t index = 0;
        int      count = 0;
        uint32_t offset = 0;
        bool     normalized = false;
    };

    struct VertexLayout {
        uint32_t stride = 0;
        std::vector<VertexAttribute> attributes;
    };

    // VertexArray: gl-style VAO abstraction combining VBO + IBO.
    class VertexArray {
    public:
        virtual ~VertexArray() = default;

        virtual void bind() const = 0;
        virtual void unbind() const = 0;

        // M1 约定：只支持 1 个 VBO + 1 个 IBO，布局由调用者提供
        virtual void setVertexBuffer(std::shared_ptr<VertexBuffer> vb, const VertexLayout& layout) = 0;
        virtual void setIndexBuffer(std::shared_ptr<IndexBuffer> ib) = 0;

        virtual uint32_t getIndexCount() const = 0;
        
        static std::shared_ptr<VertexArray> Create();
    };

} // namespace Hybrid
