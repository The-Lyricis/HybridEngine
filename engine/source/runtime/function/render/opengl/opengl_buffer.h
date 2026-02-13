#pragma once
#include "../buffer.h"
#include <cstdint>

namespace Hybrid {

    // OpenGLVertexBuffer: OpenGL implementation of VertexBuffer.
    class OpenGLVertexBuffer final : public VertexBuffer {
    public:
        OpenGLVertexBuffer(const void* data, uint32_t size);
        ~OpenGLVertexBuffer() override;

        void bind() const override;
        void unbind() const override;
        void setData(const void* data, uint32_t size) override;

    private:
        uint32_t m_RendererID = 0;
    };

    // OpenGLIndexBuffer: OpenGL implementation of IndexBuffer.
    class OpenGLIndexBuffer final : public IndexBuffer {
    public:
        OpenGLIndexBuffer(const uint32_t* indices, uint32_t count);
        ~OpenGLIndexBuffer() override;

        void bind() const override;
        void unbind() const override;

        uint32_t getCount() const override { return m_Count; }

    private:
        uint32_t m_RendererID = 0;
        uint32_t m_Count = 0;
    };

} // namespace Hybrid
