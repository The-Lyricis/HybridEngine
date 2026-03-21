#pragma once
#include "runtime/modules/render/public/buffer.h"
#include <cstdint>

namespace Hybrid {

    // GLVertexBuffer: OpenGL implementation of VertexBuffer.
    class GLVertexBuffer final : public VertexBuffer {
    public:
        GLVertexBuffer(const void* data, uint32_t size);
        ~GLVertexBuffer() override;

        void bind() const override;
        void unbind() const override;
        void setData(const void* data, uint32_t size) override;

    private:
        uint32_t m_RendererID = 0;
    };

    // GLIndexBuffer: OpenGL implementation of IndexBuffer.
    class GLIndexBuffer final : public IndexBuffer {
    public:
        GLIndexBuffer(const uint32_t* indices, uint32_t count);
        ~GLIndexBuffer() override;

        void bind() const override;
        void unbind() const override;

        uint32_t getCount() const override { return m_Count; }

    private:
        uint32_t m_RendererID = 0;
        uint32_t m_Count = 0;
    };

    class GLUniformBuffer final : public UniformBuffer {
    public:
        explicit GLUniformBuffer(uint32_t size);
        ~GLUniformBuffer() override;

        void setData(const void* data, uint32_t size, uint32_t offset = 0) override;
        void bindBase(uint32_t binding) const override;

    private:
        uint32_t m_RendererID = 0;
        uint32_t m_Size = 0;
    };

} // namespace Hybrid


