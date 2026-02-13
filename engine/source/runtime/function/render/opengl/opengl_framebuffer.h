#pragma once
#include "../framebuffer.h"

namespace Hybrid {

    // OpenGLFramebuffer: framebuffer object with color + depth-stencil.
    class OpenGLFramebuffer final : public Framebuffer {
    public:
        explicit OpenGLFramebuffer(const FramebufferSpec& spec);
        ~OpenGLFramebuffer() override;

        void bind() const override;
        void unbind() const override;
        void resize(uint32_t w, uint32_t h) override;

        uint32_t getColorAttachmentRendererID() const override { return m_ColorAttachment; }
        uint32_t getWidth() const override { return m_Spec.width; }
        uint32_t getHeight() const override { return m_Spec.height; }

    private:
        void invalidate();

    private:
        uint32_t m_FBO = 0;
        uint32_t m_ColorAttachment = 0;   // texture2D
        uint32_t m_DepthStencilRBO = 0;   // renderbuffer

        FramebufferSpec m_Spec{};
    };

} // namespace Hybrid
