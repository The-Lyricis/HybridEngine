#include "post_process_pass.h"

#include <algorithm>
#include <array>

#include "runtime/modules/render/public/buffer.h"
#include "runtime/modules/render/public/framebuffer.h"
#include "runtime/modules/render/public/render_command.h"
#include "runtime/modules/render/public/shader.h"
#include "runtime/modules/render/public/vertex_array.h"
#include "runtime/modules/render/runtime/pipeline/pipeline_state.h"
#include "runtime/modules/render/runtime/render_binding_layout.h"
#include "runtime/modules/render/runtime/render_bindings.h"
#include "runtime/modules/render/runtime/render_targets.h"
#include "runtime/modules/render/runtime/render_shaders.h"

namespace Hybrid
{
    void PostProcessPass::execute(RenderContext& context)
    {
        const std::shared_ptr<Framebuffer>& scene_framebuffer = context.scene_framebuffer;
        if (!scene_framebuffer || context.shader_library == nullptr)
            return;

        auto shader = context.shader_library->get(std::string(RenderShaders::kPostProcess.name));
        if (!shader)
            return;

        const uint32_t width = scene_framebuffer->getWidth();
        const uint32_t height = scene_framebuffer->getHeight();
        ensureInputFramebuffer(width, height);
        if (!m_InputFramebuffer)
            return;

        auto* quad = getOrCreateFullscreenQuad();
        if (!quad)
            return;

        scene_framebuffer->copyColorAttachmentTo(*m_InputFramebuffer,
                                                 RenderTargets::kSceneColorAttachment,
                                                 RenderTargets::kSceneColorAttachment);

        scene_framebuffer->bind();
        scene_framebuffer->setDrawColorAttachments({RenderTargets::kSceneColorAttachment});
        RenderCommand::setViewport(0, 0, width, height);
        ScopedPipelineState pipeline_state(PipelineStates::FullscreenNoDepth());

        const RenderBindingLayoutDesc& binding_layout = GetPostProcessBindingLayout();
        const RenderBindingDesc* scene_color_binding =
            FindRenderBinding(binding_layout, RenderBindings::kPostProcessSceneColorUniform);
        const RenderBindingDesc* tone_mapping_binding =
            FindRenderBinding(binding_layout, RenderBindings::kPostProcessToneMappingUniform);
        const RenderBindingDesc* gamma_correction_binding =
            FindRenderBinding(binding_layout, RenderBindings::kPostProcessGammaCorrectionUniform);
        const RenderBindingDesc* exposure_binding =
            FindRenderBinding(binding_layout, RenderBindings::kPostProcessExposureUniform);
        const RenderBindingDesc* gamma_binding =
            FindRenderBinding(binding_layout, RenderBindings::kPostProcessGammaUniform);
        if (!scene_color_binding || !tone_mapping_binding || !gamma_correction_binding || !exposure_binding || !gamma_binding)
            return;

        shader->bind();
        shader->setInt(std::string(tone_mapping_binding->name),
                       m_Settings.enable_tone_mapping ? 1 : 0);
        shader->setInt(std::string(gamma_correction_binding->name),
                       m_Settings.enable_gamma_correction ? 1 : 0);
        shader->setFloat(std::string(exposure_binding->name),
                         std::max(0.0f, m_Settings.exposure));
        shader->setFloat(std::string(gamma_binding->name),
                         std::max(0.0001f, m_Settings.gamma));
        m_InputFramebuffer->bindColorAttachmentTexture(RenderTargets::kSceneColorAttachment,
                                                       scene_color_binding->slot);

        quad->vao->bind();
        RenderCommand::drawIndexed(quad->index_count);

        scene_framebuffer->setDrawColorAttachments({
            RenderTargets::kSceneColorAttachment,
            RenderTargets::kSceneEntityIDAttachment
        });
        scene_framebuffer->unbind();
    }

    void PostProcessPass::ensureInputFramebuffer(uint32_t width, uint32_t height)
    {
        width = std::max(1u, width);
        height = std::max(1u, height);

        FramebufferSpec spec{};
        spec.width = width;
        spec.height = height;
        spec.attachment_spec = {
            FramebufferTextureFormat::RGBA8
        };

        if (!m_InputFramebuffer)
        {
            m_InputFramebuffer = Framebuffer::Create(spec);
            return;
        }

        if (m_InputFramebuffer->getWidth() != width || m_InputFramebuffer->getHeight() != height)
            m_InputFramebuffer->resize(width, height);
    }

    PostProcessPass::FullscreenQuadGPU* PostProcessPass::getOrCreateFullscreenQuad()
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
