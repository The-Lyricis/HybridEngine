#include "selection_overlay_pass.h"

#include <array>

#include <glad/gl.h>

#include "runtime/modules/render/public/buffer.h"
#include "runtime/modules/render/public/framebuffer.h"
#include "runtime/modules/render/public/render_command.h"
#include "runtime/modules/render/public/shader.h"
#include "runtime/modules/render/public/vertex_array.h"
#include "runtime/modules/render/runtime/render_bindings.h"
#include "runtime/modules/render/runtime/render_targets.h"

namespace Hybrid
{
    void SelectionOverlayPass::execute(RenderContext& context)
    {
        const std::shared_ptr<Framebuffer>& scene_framebuffer = context.scene_framebuffer;
        const std::shared_ptr<Framebuffer>& selection_framebuffer = context.selection_framebuffer;
        const EditorSelectionState* selection = context.editor_selection;
        const SelectionOverlayStyle* style = context.selection_overlay_style;

        if (!scene_framebuffer || !selection_framebuffer || !selection || selection->selected_entities.empty() ||
            context.shader_library == nullptr || style == nullptr)
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
        scene_framebuffer->copyColorAttachmentTo(*m_InputFramebuffer, 0, 0);
        scene_framebuffer->copyDepthAttachmentTo(*m_InputFramebuffer);

        auto* quad = getOrCreateFullscreenQuad();
        if (!quad)
            return;

        scene_framebuffer->bind();
        scene_framebuffer->setDrawColorAttachments({RenderTargets::kSceneColorAttachment});
        RenderCommand::setViewport(0, 0, width, height);

        RenderCommand::setDepthTestEnabled(false);
        RenderCommand::setDepthWriteEnabled(false);
        RenderCommand::setCullEnabled(false);
        RenderCommand::setBlendEnabled(false);

        shader->bind();
        shader->setFloat("u_TexelWidth", width > 0 ? 1.0f / static_cast<float>(width) : 0.0f);
        shader->setFloat("u_TexelHeight", height > 0 ? 1.0f / static_cast<float>(height) : 0.0f);
        shader->setFloat("u_DepthEpsilon", style->depth_epsilon);
        shader->setVec4("u_VisibleOutlineColor", style->visible_outline_color);
        shader->setVec4("u_OccludedOutlineColor", style->occluded_outline_color);
        shader->setVec4("u_FillColor", style->fill_color);

        m_InputFramebuffer->bindColorAttachmentTexture(RenderTargets::kSceneColorAttachment,
                                                      RenderBindings::kSelectionOverlaySceneColorSlot);
        m_InputFramebuffer->bindDepthAttachmentTexture(RenderBindings::kSelectionOverlaySceneDepthSlot);
        selection_framebuffer->bindColorAttachmentTexture(RenderTargets::kSelectionMaskAttachment,
                                                         RenderBindings::kSelectionOverlayMaskSlot);
        selection_framebuffer->bindDepthAttachmentTexture(RenderBindings::kSelectionOverlaySelectedDepthSlot);

        quad->vao->bind();
        RenderCommand::drawIndexed(quad->index_count);

        RenderCommand::setDepthWriteEnabled(true);
        RenderCommand::setCullEnabled(true);
        scene_framebuffer->setDrawColorAttachments({
            RenderTargets::kSceneColorAttachment,
            RenderTargets::kSceneEntityIDAttachment
        });
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
