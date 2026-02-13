#pragma once
#include <cstdint>
#include <memory>

namespace Hybrid {

    // VertexBuffer: GPU-resident vertex data blob.
    class VertexBuffer {
    public:
        virtual ~VertexBuffer() = default;

        virtual void bind() const = 0;
        virtual void unbind() const = 0;
        virtual void setData(const void* data, uint32_t size) = 0;

        static std::shared_ptr<VertexBuffer> Create(const void* data, uint32_t size);
    };

    // IndexBuffer: holds index order for indexed drawing.
    class IndexBuffer {
    public:
        virtual ~IndexBuffer() = default;

        virtual void bind() const = 0;
        virtual void unbind() const = 0;

        virtual uint32_t getCount() const = 0;

        static std::shared_ptr<IndexBuffer> Create(const uint32_t* indices, uint32_t count);
    };

} // namespace Hybrid
