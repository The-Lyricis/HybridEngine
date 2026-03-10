#include "opengl_framebuffer.h"
#include <glad/gl.h>
#include <algorithm>

namespace Hybrid {

    GLFramebuffer::GLFramebuffer(const FramebufferSpec& spec)
        : m_Spec(spec) {
        invalidate();
    }

    GLFramebuffer::~GLFramebuffer() {
        if (m_DepthStencilRBO) glDeleteRenderbuffers(1, &m_DepthStencilRBO);
        glDeleteTextures(2, m_ColorAttachments);
        if (m_FBO) glDeleteFramebuffers(1, &m_FBO);
    }

    void GLFramebuffer::invalidate() {
        // delete old resources
        if (m_DepthStencilRBO) { glDeleteRenderbuffers(1, &m_DepthStencilRBO); m_DepthStencilRBO = 0; }
        glDeleteTextures(2, m_ColorAttachments);
        m_ColorAttachments[0] = 0;
        m_ColorAttachments[1] = 0;
        if (m_FBO) { glDeleteFramebuffers(1, &m_FBO); m_FBO = 0; }

        glGenFramebuffers(1, &m_FBO);
        glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);


        // COLOR0: RGBA8
        glGenTextures(1, &m_ColorAttachments[0]);
        glBindTexture(GL_TEXTURE_2D, m_ColorAttachments[0]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, m_Spec.width, m_Spec.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_ColorAttachments[0], 0);


        // COLOR1: R32UI (EntityID)
        glGenTextures(1, &m_ColorAttachments[1]);
        glBindTexture(GL_TEXTURE_2D, m_ColorAttachments[1]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R32UI, m_Spec.width, m_Spec.height, 0, GL_RED_INTEGER, GL_UNSIGNED_INT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, m_ColorAttachments[1], 0);

        // Draw buffers
        GLenum buffers[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
        glDrawBuffers(2, buffers);


        // depth-stencil renderbuffer
        glGenRenderbuffers(1, &m_DepthStencilRBO);
        glBindRenderbuffer(GL_RENDERBUFFER, m_DepthStencilRBO);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8,
            static_cast<GLsizei>(m_Spec.width), static_cast<GLsizei>(m_Spec.height));
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_DepthStencilRBO);

        const auto status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        (void)status; // TODO: replace with logging/assert

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void GLFramebuffer::bind() const {
        glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);
    }

    void GLFramebuffer::unbind() const {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void GLFramebuffer::resize(uint32_t w, uint32_t h) {
        w = std::max(1u, w);
        h = std::max(1u, h);
        if (w == m_Spec.width && h == m_Spec.height) return;

        m_Spec.width = w;
        m_Spec.height = h;
        invalidate();
    }
    uint32_t GLFramebuffer::getColorAttachmentRendererID(uint32_t index) const
    {
        if (index >= 2) return 0;
        return m_ColorAttachments[index];
    }

} // namespace Hybrid
