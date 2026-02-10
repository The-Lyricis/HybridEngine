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

        void bind() const;
        void unbind() const;

        void resize(uint32_t w, uint32_t h);

        uint32_t getColorAttachmentRendererID() const { return m_ColorAttachment; }
        uint32_t getWidth() const { return m_Spec.width; }
        uint32_t getHeight() const { return m_Spec.height; }

    private:
        void invalidate(); // 重新创建 GPU 资源

    private:
        uint32_t m_FBO = 0;
        uint32_t m_ColorAttachment = 0;   // texture2D
        uint32_t m_DepthStencilRBO = 0;   // renderbuffer

        FramebufferSpec m_Spec{};
    };

} // namespace Hybrid
