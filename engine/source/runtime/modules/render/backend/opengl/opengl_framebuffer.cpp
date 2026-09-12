#include "opengl_framebuffer.h"

#include <algorithm>
#include <stdexcept>
#include <string>

#include <glad/gl.h>

#include "runtime/modules/render/backend/opengl/opengl_render_device_bridge.h"
#include "runtime/modules/render/rhi/render_device.h"

namespace Hybrid {
    namespace
    {
        bool isDepthFormat(FramebufferTextureFormat format)
        {
            return format == FramebufferTextureFormat::Depth24Stencil8 ||
                   format == FramebufferTextureFormat::Depth32F;
        }

        RhiFormat toRhiFormat(FramebufferTextureFormat format)
        {
            switch (format)
            {
            case FramebufferTextureFormat::RGBA8:            return RhiFormat::RGBA8Unorm;
            case FramebufferTextureFormat::R8:               return RhiFormat::R8Unorm;
            case FramebufferTextureFormat::R32UI:            return RhiFormat::R32Uint;
            case FramebufferTextureFormat::Depth24Stencil8:  return RhiFormat::Depth24Stencil8;
            case FramebufferTextureFormat::Depth32F:         return RhiFormat::Depth32Float;
            default:                                         return RhiFormat::Unknown;
            }
        }

        GLenum depthAttachmentPoint(FramebufferTextureFormat format)
        {
            return format == FramebufferTextureFormat::Depth24Stencil8
                ? GL_DEPTH_STENCIL_ATTACHMENT
                : GL_DEPTH_ATTACHMENT;
        }
    }

    GLFramebuffer::GLFramebuffer(const FramebufferSpec& spec, IRenderDevice& device)
        : m_Device(&device), m_Spec(spec)
    {
        if (device.backend() != GraphicsBackend::OpenGL)
            throw std::runtime_error("GLFramebuffer requires an OpenGL render device");
        invalidate();
    }

    GLFramebuffer::~GLFramebuffer()
    {
        if (m_FBO)
            glDeleteFramebuffers(1, &m_FBO);
        releaseAttachments();
    }

    void GLFramebuffer::releaseAttachments()
    {
        if (!m_Device)
            return;

        for (TextureViewHandle view : m_ColorAttachmentViews)
            if (view)
                (void)m_Device->destroyTextureView(view);
        for (TextureHandle texture : m_ColorAttachments)
            if (texture)
                (void)m_Device->destroyTexture(texture);
        if (m_DepthAttachmentView)
            (void)m_Device->destroyTextureView(m_DepthAttachmentView);
        if (m_DepthAttachment)
            (void)m_Device->destroyTexture(m_DepthAttachment);

        m_ColorAttachments.clear();
        m_ColorAttachmentViews.clear();
        m_ColorAttachmentNativeHandles.clear();
        m_DepthAttachment = {};
        m_DepthAttachmentView = {};
        m_DepthAttachmentNativeHandle = 0;
    }

    void GLFramebuffer::invalidate()
    {
        if (m_FBO)
        {
            glDeleteFramebuffers(1, &m_FBO);
            m_FBO = 0;
        }
        releaseAttachments();

        const auto fail = [this](const std::string& message)
        {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            if (m_FBO)
            {
                glDeleteFramebuffers(1, &m_FBO);
                m_FBO = 0;
            }
            releaseAttachments();
            throw std::runtime_error(message);
        };

        glGenFramebuffers(1, &m_FBO);
        if (!m_FBO)
            fail("OpenGL framebuffer allocation failed");
        glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);

        uint32_t color_index = 0;
        bool has_depth = false;
        for (FramebufferTextureFormat format : m_Spec.attachment_spec.attachments)
        {
            if (format == FramebufferTextureFormat::None)
                continue;

            const RhiFormat rhi_format = toRhiFormat(format);
            if (rhi_format == RhiFormat::Unknown)
                fail("framebuffer contains an unsupported attachment format");
            if (isDepthFormat(format) && has_depth)
                fail("framebuffer supports only one depth attachment");

            RhiTextureDesc desc{};
            desc.type = RhiTextureType::Texture2D;
            desc.format = rhi_format;
            desc.width = m_Spec.width;
            desc.height = m_Spec.height;
            desc.render_target = true;
            desc.shader_read = true;
            desc.debug_name = isDepthFormat(format)
                ? "LegacyFramebuffer.Depth"
                : "LegacyFramebuffer.Color" + std::to_string(color_index);

            const auto texture_result = m_Device->createTexture(desc);
            if (!texture_result)
                fail("framebuffer texture creation failed: " + texture_result.error.message);
            const auto view_result = m_Device->createTextureView(texture_result.value);
            if (!view_result)
            {
                (void)m_Device->destroyTexture(texture_result.value);
                fail("framebuffer texture view creation failed: " + view_result.error.message);
            }
            const auto native_result = ResolveOpenGLTextureNativeHandle(*m_Device, view_result.value);
            if (!native_result)
            {
                (void)m_Device->destroyTextureView(view_result.value);
                (void)m_Device->destroyTexture(texture_result.value);
                fail("framebuffer native texture resolution failed: " + native_result.error.message);
            }
            const uint32_t native_handle = static_cast<uint32_t>(native_result.value);

            if (isDepthFormat(format))
            {
                m_DepthAttachment = texture_result.value;
                m_DepthAttachmentView = view_result.value;
                m_DepthAttachmentNativeHandle = native_handle;
                glFramebufferTexture2D(GL_FRAMEBUFFER,
                                       depthAttachmentPoint(format),
                                       GL_TEXTURE_2D,
                                       native_handle,
                                       0);
                has_depth = true;
                continue;
            }

            m_ColorAttachments.push_back(texture_result.value);
            m_ColorAttachmentViews.push_back(view_result.value);
            m_ColorAttachmentNativeHandles.push_back(native_handle);
            glFramebufferTexture2D(GL_FRAMEBUFFER,
                                   GL_COLOR_ATTACHMENT0 + color_index,
                                   GL_TEXTURE_2D,
                                   native_handle,
                                   0);
            ++color_index;
        }

        if (!m_ColorAttachmentNativeHandles.empty())
        {
            std::vector<GLenum> draw_buffers(m_ColorAttachmentNativeHandles.size());
            for (uint32_t i = 0; i < static_cast<uint32_t>(draw_buffers.size()); ++i)
                draw_buffers[i] = GL_COLOR_ATTACHMENT0 + i;
            glDrawBuffers(static_cast<GLsizei>(draw_buffers.size()), draw_buffers.data());
        }
        else
        {
            glDrawBuffer(GL_NONE);
            glReadBuffer(GL_NONE);
        }

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            fail("OpenGL framebuffer is incomplete");
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void GLFramebuffer::bind() const
    {
        glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);
    }

    void GLFramebuffer::unbind() const
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void GLFramebuffer::resize(uint32_t w, uint32_t h)
    {
        w = std::max(1u, w);
        h = std::max(1u, h);
        if (w == m_Spec.width && h == m_Spec.height)
            return;
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
        if (index >= m_ColorAttachmentNativeHandles.size())
            return;
        bind();
        glClearBufferuiv(GL_COLOR, static_cast<GLint>(index), &value);
    }

    uint32_t GLFramebuffer::readPixelUInt(uint32_t attachment_index, int x, int y) const
    {
        if (attachment_index >= m_ColorAttachmentNativeHandles.size())
            return 0;
        bind();
        glReadBuffer(GL_COLOR_ATTACHMENT0 + attachment_index);
        uint32_t value = 0;
        glReadPixels(x, y, 1, 1, GL_RED_INTEGER, GL_UNSIGNED_INT, &value);
        return value;
    }

    void GLFramebuffer::copyColorAttachmentTo(const Framebuffer& dst,
                                              uint32_t src_index,
                                              uint32_t dst_index) const
    {
        const auto* dst_gl = dynamic_cast<const GLFramebuffer*>(&dst);
        if (!dst_gl || dst_gl->m_Device != m_Device)
            throw std::runtime_error("copyColorAttachmentTo requires a framebuffer from the same OpenGL device");
        if (src_index >= m_ColorAttachmentNativeHandles.size() ||
            dst_index >= dst_gl->m_ColorAttachmentNativeHandles.size())
            return;

        glBindFramebuffer(GL_READ_FRAMEBUFFER, m_FBO);
        glReadBuffer(GL_COLOR_ATTACHMENT0 + src_index);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, dst_gl->m_FBO);
        glDrawBuffer(GL_COLOR_ATTACHMENT0 + dst_index);
        const GLsizei width = static_cast<GLsizei>(std::min(m_Spec.width, dst_gl->m_Spec.width));
        const GLsizei height = static_cast<GLsizei>(std::min(m_Spec.height, dst_gl->m_Spec.height));
        glBlitFramebuffer(0, 0, width, height, 0, 0, width, height,
                          GL_COLOR_BUFFER_BIT, GL_NEAREST);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void GLFramebuffer::copyDepthAttachmentTo(const Framebuffer& dst) const
    {
        const auto* dst_gl = dynamic_cast<const GLFramebuffer*>(&dst);
        if (!dst_gl || dst_gl->m_Device != m_Device)
            throw std::runtime_error("copyDepthAttachmentTo requires a framebuffer from the same OpenGL device");
        if (!m_DepthAttachmentNativeHandle || !dst_gl->m_DepthAttachmentNativeHandle)
            return;

        glBindFramebuffer(GL_READ_FRAMEBUFFER, m_FBO);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, dst_gl->m_FBO);
        const GLsizei width = static_cast<GLsizei>(std::min(m_Spec.width, dst_gl->m_Spec.width));
        const GLsizei height = static_cast<GLsizei>(std::min(m_Spec.height, dst_gl->m_Spec.height));
        glBlitFramebuffer(0, 0, width, height, 0, 0, width, height,
                          GL_DEPTH_BUFFER_BIT, GL_NEAREST);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void GLFramebuffer::bindColorAttachmentTexture(uint32_t index, uint32_t slot) const
    {
        if (index >= m_ColorAttachmentNativeHandles.size())
            return;
        glActiveTexture(GL_TEXTURE0 + slot);
        glBindTexture(GL_TEXTURE_2D, m_ColorAttachmentNativeHandles[index]);
    }

    void GLFramebuffer::bindDepthAttachmentTexture(uint32_t slot) const
    {
        if (!m_DepthAttachmentNativeHandle)
            return;
        glActiveTexture(GL_TEXTURE0 + slot);
        glBindTexture(GL_TEXTURE_2D, m_DepthAttachmentNativeHandle);
    }

    TextureViewHandle GLFramebuffer::getColorAttachmentView(uint32_t index) const
    {
        return index < m_ColorAttachmentViews.size()
            ? m_ColorAttachmentViews[index]
            : TextureViewHandle{};
    }
} // namespace Hybrid
