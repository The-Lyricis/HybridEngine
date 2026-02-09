#pragma once
#include <cstdint>

namespace Hybrid {

    struct FramebufferSpec {
        uint32_t width = 1280;
        uint32_t height = 720;
    };

    class Framebuffer {
    public:
        explicit Framebuffer(const FramebufferSpec& spec);
        ~Framebuffer();

        void Bind() const;
        void Unbind() const;

        void Resize(uint32_t w, uint32_t h);

        uint32_t GetColorAttachmentRendererID() const { return m_ColorAttachment; }
        uint32_t GetWidth() const { return m_Spec.width; }
        uint32_t GetHeight() const { return m_Spec.height; }

    private:
        void Invalidate(); // 重新创建 GPU 资源

    private:
        uint32_t m_FBO = 0;
        uint32_t m_ColorAttachment = 0;   // texture2D
        uint32_t m_DepthStencilRBO = 0;   // renderbuffer

        FramebufferSpec m_Spec{};
    };

} // namespace Hybrid
