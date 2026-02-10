#include "framebuffer.h"
#include <glad/gl.h>
#include <algorithm>

namespace Hybrid {

    Framebuffer::Framebuffer(const FramebufferSpec& spec)
        : m_Spec(spec) {
        invalidate();
    }

    Framebuffer::~Framebuffer() {
        if (m_DepthStencilRBO) glDeleteRenderbuffers(1, &m_DepthStencilRBO);
        if (m_ColorAttachment) glDeleteTextures(1, &m_ColorAttachment);
        if (m_FBO) glDeleteFramebuffers(1, &m_FBO);
    }

    void Framebuffer::invalidate() {
        // 删除旧资源
        if (m_DepthStencilRBO) { glDeleteRenderbuffers(1, &m_DepthStencilRBO); m_DepthStencilRBO = 0; }
        if (m_ColorAttachment) { glDeleteTextures(1, &m_ColorAttachment); m_ColorAttachment = 0; }
        if (m_FBO) { glDeleteFramebuffers(1, &m_FBO); m_FBO = 0; }

        glGenFramebuffers(1, &m_FBO);
        glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);

        // 1) Color attachment texture
        glGenTextures(1, &m_ColorAttachment);
        glBindTexture(GL_TEXTURE_2D, m_ColorAttachment);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
            (GLsizei)m_Spec.width, (GLsizei)m_Spec.height,
            0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_ColorAttachment, 0);

        // 2) Depth-stencil renderbuffer
        glGenRenderbuffers(1, &m_DepthStencilRBO);
        glBindRenderbuffer(GL_RENDERBUFFER, m_DepthStencilRBO);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8,
            (GLsizei)m_Spec.width, (GLsizei)m_Spec.height);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_DepthStencilRBO);

        // 3) Check status
        const auto status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        // 可用日志系统替换
        // HBD_CORE_ASSERT(status == GL_FRAMEBUFFER_COMPLETE, "Framebuffer incomplete!");
        (void)status;

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void Framebuffer::bind() const {
        glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);
    }

    void Framebuffer::unbind() const {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void Framebuffer::resize(uint32_t w, uint32_t h) {
        w = std::max(1u, w);
        h = std::max(1u, h);
        if (w == m_Spec.width && h == m_Spec.height) return;

        m_Spec.width = w;
        m_Spec.height = h;
        invalidate();
    }

} // namespace Hybrid
