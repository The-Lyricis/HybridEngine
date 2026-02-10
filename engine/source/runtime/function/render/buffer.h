#pragma once
#include <cstdint>

namespace Hybrid {

    class VertexBuffer {
    public:
        VertexBuffer(const void* data, uint32_t size);
        ~VertexBuffer();

        void bind() const;
        void unbind() const;

    private:
        uint32_t m_RendererID = 0;
    };

    class IndexBuffer {
    public:
        IndexBuffer(const uint32_t* indices, uint32_t count);
        ~IndexBuffer();

        void bind() const;
        void unbind() const;

        uint32_t getCount() const { return m_Count; }

    private:
        uint32_t m_RendererID = 0;
        uint32_t m_Count = 0;
    };

} // namespace Hybrid
