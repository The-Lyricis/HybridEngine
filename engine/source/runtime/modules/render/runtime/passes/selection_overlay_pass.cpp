#include "selection_overlay_pass.h"

#include <array>

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

    void SelectionOverlayPass::execute(RenderContext& context)
    {
        const std::shared_ptr<Framebuffer>& scene_framebuffer = context.scene_framebuffer;
        const std::shared_ptr<Framebuffer>& selection_framebuffer = context.selection_framebuffer;
        const EditorSelectionState* selection = context.editor_selection;

        if (!scene_framebuffer || !selection_framebuffer || !selection || selection->selected_entities.empty() ||
            context.shader_library == nullptr)
        {
            return;
        }

        auto shader = context.shader_library->get("SelectionOverlay");
        if (!shader)
            return;

        ensureInputFramebuffer(scene_framebuffer->getWidth(), scene_framebuffer->getHeight());
        if (!m_InputFramebuffer)
            return;

        const uint32_t width = scene_framebuffer->getWidth();
        const uint32_t height = scene_framebuffer->getHeight();

        // Snapshot the scene inputs first to avoid sampling from the same target
        // that this pass writes back into.
        glCopyImageSubData(scene_framebuffer->getColorAttachmentRendererID(0), GL_TEXTURE_2D, 0, 0, 0, 0,
                           m_InputFramebuffer->getColorAttachmentRendererID(0), GL_TEXTURE_2D, 0, 0, 0, 0,
                           static_cast<GLsizei>(width), static_cast<GLsizei>(height), 1);
        glCopyImageSubData(scene_framebuffer->getDepthAttachmentRendererID(), GL_TEXTURE_2D, 0, 0, 0, 0,
                           m_InputFramebuffer->getDepthAttachmentRendererID(), GL_TEXTURE_2D, 0, 0, 0, 0,
                           static_cast<GLsizei>(width), static_cast<GLsizei>(height), 1);

        auto* quad = getOrCreateFullscreenQuad();
        if (!quad)
            return;

        scene_framebuffer->bind();
        setSceneFramebufferDrawBuffers(false);
        RenderCommand::setViewport(0, 0, width, height);

        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        glDisable(GL_CULL_FACE);
        glDisable(GL_BLEND);

        shader->bind();
        shader->setInt("u_SceneColorTex", 0);
        shader->setInt("u_SceneDepthTex", 1);
        shader->setInt("u_SelectedMaskTex", 2);
        shader->setInt("u_SelectedDepthTex", 3);
        shader->setFloat("u_TexelWidth", width > 0 ? 1.0f / static_cast<float>(width) : 0.0f);
        shader->setFloat("u_TexelHeight", height > 0 ? 1.0f / static_cast<float>(height) : 0.0f);
        shader->setFloat("u_DepthEpsilon", 1e-5f);
        shader->setVec4("u_VisibleOutlineColor", glm::vec4(0.836f, 0.292f, 0.312f, 0.95f));
        shader->setVec4("u_OccludedOutlineColor", glm::vec4(0.320f, 0.360f, 0.500f, 0.40f));
        shader->setVec4("u_FillColor", glm::vec4(0.836f, 0.292f, 0.312f, 0.12f));

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_InputFramebuffer->getColorAttachmentRendererID(0));
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, m_InputFramebuffer->getDepthAttachmentRendererID());
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, selection_framebuffer->getColorAttachmentRendererID(0));
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, selection_framebuffer->getDepthAttachmentRendererID());

        quad->vao->bind();
        RenderCommand::drawIndexed(quad->index_count);

        glDepthMask(GL_TRUE);
        glEnable(GL_CULL_FACE);
        setSceneFramebufferDrawBuffers(true);
        scene_framebuffer->unbind();
    }

    void SelectionOverlayPass::ensureInputFramebuffer(uint32_t width, uint32_t height)
    {
        width = std::max(1u, width);
        height = std::max(1u, height);

        FramebufferSpec spec{};
        spec.width = width;
        spec.height = height;
        spec.attachment_spec = {
            FramebufferTextureFormat::RGBA8,
            FramebufferTextureFormat::Depth32F
        };

        if (!m_InputFramebuffer)
        {
            m_InputFramebuffer = Framebuffer::Create(spec);
            return;
        }

        if (m_InputFramebuffer->getWidth() != width || m_InputFramebuffer->getHeight() != height)
            m_InputFramebuffer->resize(width, height);
    }

    SelectionOverlayPass::FullscreenQuadGPU* SelectionOverlayPass::getOrCreateFullscreenQuad()
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
