#include "opengl_framebuffer.h"

#include <algorithm>
#include <stdexcept>

#include <glad/gl.h>

namespace Hybrid {
    namespace
    {
        bool isDepthFormat(FramebufferTextureFormat format)
        {
            return format == FramebufferTextureFormat::Depth24Stencil8 ||
                   format == FramebufferTextureFormat::Depth32F;
        }

        GLenum toGLInternalFormat(FramebufferTextureFormat format)
        {
            switch (format)
            {
            case FramebufferTextureFormat::RGBA8:
                return GL_RGBA8;
            case FramebufferTextureFormat::R8:
                return GL_R8;
            case FramebufferTextureFormat::R32UI:
                return GL_R32UI;
            case FramebufferTextureFormat::Depth24Stencil8:
                return GL_DEPTH24_STENCIL8;
            case FramebufferTextureFormat::Depth32F:
                return GL_DEPTH_COMPONENT32F;
            default:
                return 0;
            }
        }

        GLenum toGLDataFormat(FramebufferTextureFormat format)
        {
            switch (format)
            {
            case FramebufferTextureFormat::RGBA8:
                return GL_RGBA;
            case FramebufferTextureFormat::R8:
                return GL_RED;
            case FramebufferTextureFormat::R32UI:
                return GL_RED_INTEGER;
            case FramebufferTextureFormat::Depth24Stencil8:
                return GL_DEPTH_STENCIL;
            case FramebufferTextureFormat::Depth32F:
                return GL_DEPTH_COMPONENT;
            default:
                return 0;
            }
        }

        GLenum toGLDataType(FramebufferTextureFormat format)
        {
            switch (format)
            {
            case FramebufferTextureFormat::RGBA8:
            case FramebufferTextureFormat::R8:
                return GL_UNSIGNED_BYTE;
            case FramebufferTextureFormat::R32UI:
                return GL_UNSIGNED_INT;
            case FramebufferTextureFormat::Depth24Stencil8:
                return GL_UNSIGNED_INT_24_8;
            case FramebufferTextureFormat::Depth32F:
                return GL_FLOAT;
            default:
                return 0;
            }
        }

        GLenum toGLAttachmentPoint(FramebufferTextureFormat format)
        {
            switch (format)
            {
            case FramebufferTextureFormat::Depth24Stencil8:
                return GL_DEPTH_STENCIL_ATTACHMENT;
            case FramebufferTextureFormat::Depth32F:
                return GL_DEPTH_ATTACHMENT;
            default:
                return 0;
            }
        }

        void configureColorTexture(GLenum target, FramebufferTextureFormat format)
        {
            const bool integer_texture = (format == FramebufferTextureFormat::R32UI);
            const bool single_channel = (format == FramebufferTextureFormat::R8);

            glTexParameteri(target, GL_TEXTURE_MIN_FILTER, integer_texture || single_channel ? GL_NEAREST : GL_LINEAR);
            glTexParameteri(target, GL_TEXTURE_MAG_FILTER, integer_texture || single_channel ? GL_NEAREST : GL_LINEAR);
            glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        }

        void configureDepthTexture(GLenum target)
        {
            glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(target, GL_TEXTURE_COMPARE_MODE, GL_NONE);
        }
    }

    GLFramebuffer::GLFramebuffer(const FramebufferSpec& spec)
        : m_Spec(spec)
    {
        invalidate();
    }

    GLFramebuffer::~GLFramebuffer()
    {
        if (!m_ColorAttachments.empty())
            glDeleteTextures(static_cast<GLsizei>(m_ColorAttachments.size()), m_ColorAttachments.data());
        if (m_DepthAttachment)
            glDeleteTextures(1, &m_DepthAttachment);
        if (m_FBO)
            glDeleteFramebuffers(1, &m_FBO);
    }

    void GLFramebuffer::invalidate()
    {
        if (!m_ColorAttachments.empty())
        {
            glDeleteTextures(static_cast<GLsizei>(m_ColorAttachments.size()), m_ColorAttachments.data());
            m_ColorAttachments.clear();
        }
        if (m_DepthAttachment)
        {
            glDeleteTextures(1, &m_DepthAttachment);
            m_DepthAttachment = 0;
        }
        if (m_FBO)
        {
            glDeleteFramebuffers(1, &m_FBO);
            m_FBO = 0;
        }

        glGenFramebuffers(1, &m_FBO);
        glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);

        uint32_t color_index = 0;
        for (FramebufferTextureFormat format : m_Spec.attachment_spec.attachments)
        {
            if (format == FramebufferTextureFormat::None)
                continue;

            const GLenum internal_format = toGLInternalFormat(format);
            const GLenum data_format = toGLDataFormat(format);
            const GLenum data_type = toGLDataType(format);
            if (internal_format == 0 || data_format == 0 || data_type == 0)
                continue;

            if (isDepthFormat(format))
            {
                glGenTextures(1, &m_DepthAttachment);
                glBindTexture(GL_TEXTURE_2D, m_DepthAttachment);
                glTexImage2D(GL_TEXTURE_2D,
                             0,
                             internal_format,
                             static_cast<GLsizei>(m_Spec.width),
                             static_cast<GLsizei>(m_Spec.height),
                             0,
                             data_format,
                             data_type,
                             nullptr);
                configureDepthTexture(GL_TEXTURE_2D);
                glFramebufferTexture2D(GL_FRAMEBUFFER,
                                       toGLAttachmentPoint(format),
                                       GL_TEXTURE_2D,
                                       m_DepthAttachment,
                                       0);
                continue;
            }

            uint32_t texture = 0;
            glGenTextures(1, &texture);
            glBindTexture(GL_TEXTURE_2D, texture);
            glTexImage2D(GL_TEXTURE_2D,
                         0,
                         internal_format,
                         static_cast<GLsizei>(m_Spec.width),
                         static_cast<GLsizei>(m_Spec.height),
                         0,
                         data_format,
                         data_type,
                         nullptr);
            configureColorTexture(GL_TEXTURE_2D, format);
            glFramebufferTexture2D(GL_FRAMEBUFFER,
                                   GL_COLOR_ATTACHMENT0 + color_index,
                                   GL_TEXTURE_2D,
                                   texture,
                                   0);
            m_ColorAttachments.push_back(texture);
            ++color_index;
        }

        if (!m_ColorAttachments.empty())
        {
            std::vector<GLenum> draw_buffers(m_ColorAttachments.size());
            for (uint32_t i = 0; i < static_cast<uint32_t>(m_ColorAttachments.size()); ++i)
                draw_buffers[i] = GL_COLOR_ATTACHMENT0 + i;
            glDrawBuffers(static_cast<GLsizei>(draw_buffers.size()), draw_buffers.data());
        }
        else
        {
            glDrawBuffer(GL_NONE);
            glReadBuffer(GL_NONE);
        }

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

    void GLFramebuffer::setDrawColorAttachments(std::initializer_list<uint32_t> indices) const
    {
        bind();
        if (indices.size() == 0)
        {
            glDrawBuffer(GL_NONE);
            return;
        }

        std::vector<GLenum> draw_buffers;
        draw_buffers.reserve(indices.size());
        for (uint32_t index : indices)
            draw_buffers.push_back(GL_COLOR_ATTACHMENT0 + index);

        glDrawBuffers(static_cast<GLsizei>(draw_buffers.size()), draw_buffers.data());
    }

    void GLFramebuffer::clearColorAttachmentUInt(uint32_t index, uint32_t value) const
    {
        if (index >= m_ColorAttachments.size())
            return;

        bind();
        glClearBufferuiv(GL_COLOR, static_cast<GLint>(index), &value);
    }

    uint32_t GLFramebuffer::readPixelUInt(uint32_t attachment_index, int x, int y) const
    {
        if (attachment_index >= m_ColorAttachments.size())
            return 0;

        bind();
        glReadBuffer(GL_COLOR_ATTACHMENT0 + attachment_index);

        uint32_t value = 0;
        glReadPixels(x, y, 1, 1, GL_RED_INTEGER, GL_UNSIGNED_INT, &value);
        return value;
    }

    void GLFramebuffer::copyColorAttachmentTo(const Framebuffer& dst, uint32_t src_index, uint32_t dst_index) const
    {
        const auto* dst_gl = dynamic_cast<const GLFramebuffer*>(&dst);
        if (!dst_gl)
            throw std::runtime_error("copyColorAttachmentTo requires GLFramebuffer destination");
        if (src_index >= m_ColorAttachments.size() || dst_index >= dst_gl->m_ColorAttachments.size())
            return;

        glCopyImageSubData(m_ColorAttachments[src_index], GL_TEXTURE_2D, 0, 0, 0, 0,
                           dst_gl->m_ColorAttachments[dst_index], GL_TEXTURE_2D, 0, 0, 0, 0,
                           static_cast<GLsizei>(std::min(m_Spec.width, dst_gl->m_Spec.width)),
                           static_cast<GLsizei>(std::min(m_Spec.height, dst_gl->m_Spec.height)),
                           1);
    }

    void GLFramebuffer::copyDepthAttachmentTo(const Framebuffer& dst) const
    {
        const auto* dst_gl = dynamic_cast<const GLFramebuffer*>(&dst);
        if (!dst_gl)
            throw std::runtime_error("copyDepthAttachmentTo requires GLFramebuffer destination");
        if (!m_DepthAttachment || !dst_gl->m_DepthAttachment)
            return;

        glCopyImageSubData(m_DepthAttachment, GL_TEXTURE_2D, 0, 0, 0, 0,
                           dst_gl->m_DepthAttachment, GL_TEXTURE_2D, 0, 0, 0, 0,
                           static_cast<GLsizei>(std::min(m_Spec.width, dst_gl->m_Spec.width)),
                           static_cast<GLsizei>(std::min(m_Spec.height, dst_gl->m_Spec.height)),
                           1);
    }

    void GLFramebuffer::bindColorAttachmentTexture(uint32_t index, uint32_t slot) const
    {
        if (index >= m_ColorAttachments.size())
            return;

        glActiveTexture(GL_TEXTURE0 + slot);
        glBindTexture(GL_TEXTURE_2D, m_ColorAttachments[index]);
    }

    void GLFramebuffer::bindDepthAttachmentTexture(uint32_t slot) const
    {
        if (!m_DepthAttachment)
            return;

        glActiveTexture(GL_TEXTURE0 + slot);
        glBindTexture(GL_TEXTURE_2D, m_DepthAttachment);
    }

    uint32_t GLFramebuffer::getColorAttachmentRendererID(uint32_t index) const
    {
        if (index >= m_ColorAttachments.size())
            return 0;
        return m_ColorAttachments[index];
    }

} // namespace Hybrid
