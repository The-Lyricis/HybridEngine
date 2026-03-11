#include "selection_outline_pass.h"

#include <algorithm>
#include <array>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <glad/gl.h>

#include "runtime/modules/render/public/buffer.h"
#include "runtime/modules/render/public/framebuffer.h"
#include "runtime/modules/render/public/render_command.h"
#include "runtime/modules/render/public/shader.h"
#include "runtime/modules/render/public/vertex_array.h"

namespace Hybrid
{
    namespace
    {
        void setSceneFramebufferDrawBuffers(bool write_entity_id)
        {
            if (write_entity_id)
            {
                constexpr GLenum buffers[2] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
                glDrawBuffers(2, buffers);
            }
            else
            {
                constexpr GLenum buffer = GL_COLOR_ATTACHMENT0;
                glDrawBuffers(1, &buffer);
            }
        }
    }

    void SelectionOutlinePass::execute(RenderContext& context)
    {
        const RenderPacket& packet = *context.packet;
        const std::shared_ptr<Framebuffer>& framebuffer = context.framebuffer;
        if (!framebuffer || packet.selectedEntityID == kInvalidEntityID || context.shader_library == nullptr)
            return;

        auto shader = context.shader_library->get("SelectionOutline");
        if (!shader)
            return;

        ensureInputFramebuffer(framebuffer->getWidth(), framebuffer->getHeight());
        if (!m_InputFramebuffer)
            return;

        const uint32_t width = framebuffer->getWidth();
        const uint32_t height = framebuffer->getHeight();
        const uint32_t src_color = framebuffer->getColorAttachmentRendererID(0);
        const uint32_t src_id = framebuffer->getColorAttachmentRendererID(1);
        const uint32_t copy_color = m_InputFramebuffer->getColorAttachmentRendererID(0);
        const uint32_t copy_id = m_InputFramebuffer->getColorAttachmentRendererID(1);

        glCopyImageSubData(src_color, GL_TEXTURE_2D, 0, 0, 0, 0,
                           copy_color, GL_TEXTURE_2D, 0, 0, 0, 0,
                           static_cast<GLsizei>(width), static_cast<GLsizei>(height), 1);
        glCopyImageSubData(src_id, GL_TEXTURE_2D, 0, 0, 0, 0,
                           copy_id, GL_TEXTURE_2D, 0, 0, 0, 0,
                           static_cast<GLsizei>(width), static_cast<GLsizei>(height), 1);

        auto* quad = getOrCreateFullscreenQuad();
        if (!quad)
            return;

        framebuffer->bind();
        setSceneFramebufferDrawBuffers(false);
        RenderCommand::setViewport(0, 0, width, height);

        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        shader->bind();
        shader->setInt("u_EntityIDTex", 0);
        shader->setUInt("u_SelectedEntityID", packet.selectedEntityID + 1u);
        shader->setFloat("u_TexelWidth", width > 0 ? 1.0f / static_cast<float>(width) : 0.0f);
        shader->setFloat("u_TexelHeight", height > 0 ? 1.0f / static_cast<float>(height) : 0.0f);
        shader->setVec4("u_OutlineColor", glm::vec4(1.0f, 0.62f, 0.18f, 0.95f));

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, copy_id);

        quad->vao->bind();
        RenderCommand::drawIndexed(quad->index_count);

        glDisable(GL_BLEND);
        glEnable(GL_CULL_FACE);
        setSceneFramebufferDrawBuffers(true);
        framebuffer->unbind();
    }

    void SelectionOutlinePass::ensureInputFramebuffer(uint32_t width, uint32_t height)
    {
        width = std::max(1u, width);
        height = std::max(1u, height);

        if (!m_InputFramebuffer)
        {
            FramebufferSpec spec{};
            spec.width = width;
            spec.height = height;
            m_InputFramebuffer = Framebuffer::Create(spec);
            return;
        }

        if (m_InputFramebuffer->getWidth() != width || m_InputFramebuffer->getHeight() != height)
            m_InputFramebuffer->resize(width, height);
    }

    SelectionOutlinePass::FullscreenQuadGPU* SelectionOutlinePass::getOrCreateFullscreenQuad()
    {
        if (m_HasFullscreenQuad)
            return &m_FullscreenQuad;

        struct FullscreenVertex
        {
            float position[2];
            float uv[2];
        };

        static constexpr std::array<FullscreenVertex, 4> kVertices = {
            FullscreenVertex{{-1.0f, -1.0f}, {0.0f, 0.0f}},
            FullscreenVertex{{ 1.0f, -1.0f}, {1.0f, 0.0f}},
            FullscreenVertex{{ 1.0f,  1.0f}, {1.0f, 1.0f}},
            FullscreenVertex{{-1.0f,  1.0f}, {0.0f, 1.0f}},
        };

        static constexpr std::array<uint32_t, 6> kIndices = {0, 1, 2, 2, 3, 0};

        m_FullscreenQuad.vb =
            VertexBuffer::Create(kVertices.data(), static_cast<uint32_t>(kVertices.size() * sizeof(FullscreenVertex)));
        m_FullscreenQuad.ib = IndexBuffer::Create(kIndices.data(), static_cast<uint32_t>(kIndices.size()));
        m_FullscreenQuad.vao = VertexArray::Create();

        VertexLayout layout;
        layout.stride = sizeof(FullscreenVertex);
        layout.attributes = {
            {0, 2, 0, false},
            {1, 2, sizeof(float) * 2, false},
        };

        m_FullscreenQuad.vao->setVertexBuffer(m_FullscreenQuad.vb, layout);
        m_FullscreenQuad.vao->setIndexBuffer(m_FullscreenQuad.ib);
        m_FullscreenQuad.index_count = static_cast<uint32_t>(kIndices.size());
        m_HasFullscreenQuad = true;
        return &m_FullscreenQuad;
    }
} // namespace Hybrid
