#include "shadow_pass.h"

#include "runtime/modules/render/public/framebuffer.h"
#include "runtime/modules/render/public/render_command.h"
#include "runtime/modules/render/public/shader.h"
#include "runtime/modules/render/public/vertex_array.h"
#include "runtime/modules/render/runtime/render_bindings.h"

namespace Hybrid
{
    void ShadowPass::execute(RenderContext& context)
    {
        const RenderPacket& packet = *context.packet;
        const std::shared_ptr<Shader>& shadow_shader = context.shadow_shader;

        if (!packet.shadow.enabled || packet.shadow.cascadeCount == 0 || !context.shadow_cascade_framebuffers || !shadow_shader)
            return;

        RenderCommand::setBlendEnabled(false);
        RenderCommand::setDepthTestEnabled(true);
        RenderCommand::setDepthWriteEnabled(true);
        RenderCommand::setDepthCompareFunc(DepthCompareFunc::Less);
        RenderCommand::setCullEnabled(true);
        shadow_shader->bind();
        shadow_shader->setInt(RenderBindings::kSceneAlbedoUniform, RenderBindings::kSceneAlbedoSlot);

        for (uint32_t cascade_index = 0; cascade_index < packet.shadow.cascadeCount; ++cascade_index)
        {
            const auto& cascade = packet.shadow.cascades[cascade_index];
            const std::shared_ptr<Framebuffer>& framebuffer = (*context.shadow_cascade_framebuffers)[cascade_index];
            if (!cascade.valid || !framebuffer)
                continue;

            framebuffer->bind();
            framebuffer->setDrawColorAttachments({});
            RenderCommand::setViewport(0, 0, framebuffer->getWidth(), framebuffer->getHeight());
            RenderCommand::setClearDepth(1.0f);
            RenderCommand::clear();

            shadow_shader->setMat4("u_LightViewProjection", cascade.lightViewProjection);

            for (const RenderDrawItem& item : packet.shadow_caster_items)
            {
                if (!item.meshGPU || !item.materialGPU || item.indexCount == 0)
                    continue;

                shadow_shader->setMat4("u_Model", item.model);
                shadow_shader->setVec4("u_TintColor", item.tint);
                item.materialGPU->bind(*shadow_shader);
                item.meshGPU->vao->bind();
                RenderCommand::drawIndexed(item.indexCount, item.indexOffset);
            }

            framebuffer->unbind();
        }
    }
} // namespace Hybrid
