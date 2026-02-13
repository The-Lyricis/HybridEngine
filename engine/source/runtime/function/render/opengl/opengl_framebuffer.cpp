#include "opengl_framebuffer.h"
#include <glad/gl.h>
#include <algorithm>

namespace Hybrid {

    OpenGLFramebuffer::OpenGLFramebuffer(const FramebufferSpec& spec)
        : m_Spec(spec) {
        invalidate();
    }

    OpenGLFramebuffer::~OpenGLFramebuffer() {
        if (m_DepthStencilRBO) glDeleteRenderbuffers(1, &m_DepthStencilRBO);
        if (m_ColorAttachment) glDeleteTextures(1, &m_ColorAttachment);
        if (m_FBO) glDeleteFramebuffers(1, &m_FBO);
    }

    void OpenGLFramebuffer::invalidate() {
        // delete old resources
        if (m_DepthStencilRBO) { glDeleteRenderbuffers(1, &m_DepthStencilRBO); m_DepthStencilRBO = 0; }
        if (m_ColorAttachment) { glDeleteTextures(1, &m_ColorAttachment); m_ColorAttachment = 0; }
        if (m_FBO) { glDeleteFramebuffers(1, &m_FBO); m_FBO = 0; }

        glGenFramebuffers(1, &m_FBO);
        glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);

        // color attachment
        glGenTextures(1, &m_ColorAttachment);
        glBindTexture(GL_TEXTURE_2D, m_ColorAttachment);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
            static_cast<GLsizei>(m_Spec.width), static_cast<GLsizei>(m_Spec.height),
            0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_ColorAttachment, 0);

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

    void OpenGLFramebuffer::bind() const {
        glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);
    }

    void OpenGLFramebuffer::unbind() const {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void OpenGLFramebuffer::resize(uint32_t w, uint32_t h) {
        w = std::max(1u, w);
        h = std::max(1u, h);
        if (w == m_Spec.width && h == m_Spec.height) return;

        m_Spec.width = w;
        m_Spec.height = h;
        invalidate();
    }

} // namespace Hybrid
