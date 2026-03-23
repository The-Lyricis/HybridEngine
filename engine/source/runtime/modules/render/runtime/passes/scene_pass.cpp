#include "scene_pass.h"

#include <string>

#include "runtime/modules/render/public/framebuffer.h"
#include "runtime/modules/render/public/render_command.h"
#include "runtime/modules/render/public/renderer.h"
#include "runtime/modules/render/public/shader.h"
#include "runtime/modules/render/runtime/render_bindings.h"
#include "runtime/modules/render/runtime/render_targets.h"

namespace Hybrid
{
    namespace
    {
        uint32_t encodeEntityID(uint32_t entity_id)
        {
            return entity_id + 1u;
        }
    }

    void ScenePass::execute(RenderContext& context)
    {
        const RenderPacket& packet = *context.packet;
        const std::shared_ptr<Framebuffer>& framebuffer = context.framebuffer;
        const std::shared_ptr<Shader>& scene_shader = context.scene_shader;

        if (!framebuffer)
            return;

        framebuffer->bind();
        framebuffer->setDrawColorAttachments({
            RenderTargets::kSceneColorAttachment,
            RenderTargets::kSceneEntityIDAttachment
        });
        RenderCommand::setViewport(0, 0, framebuffer->getWidth(), framebuffer->getHeight());
        Renderer::beginFrame(packet.frame.clearColor);
        framebuffer->clearColorAttachmentUInt(RenderTargets::kSceneEntityIDAttachment, 0);

        if (scene_shader)
        {
            scene_shader->bind();
            const bool has_shadow_cascade = packet.shadow.enabled && packet.shadow.cascadeCount > 0 &&
                                            packet.shadow.cascades[0].valid;
            scene_shader->setInt("u_ShadowCascadeCount", has_shadow_cascade ? static_cast<int>(packet.shadow.cascadeCount) : 0);
            for (uint32_t cascade_index = 0; cascade_index < kMaxDirectionalShadowCascades; ++cascade_index)
            {
                const bool valid = cascade_index < packet.shadow.cascadeCount && packet.shadow.cascades[cascade_index].valid;
                scene_shader->setMat4("u_LightViewProjections[" + std::to_string(cascade_index) + "]",
                                      valid ? packet.shadow.cascades[cascade_index].lightViewProjection : glm::mat4(1.0f));
                scene_shader->setFloat("u_ShadowCascadeSplits[" + std::to_string(cascade_index) + "]",
                                       valid ? packet.shadow.cascades[cascade_index].splitFar : 0.0f);
            }
            scene_shader->setInt("u_ShadowsEnabled",
                                 (has_shadow_cascade && context.shadow_cascade_framebuffers != nullptr) ? 1 : 0);
            scene_shader->setFloat("u_ShadowStrength", packet.shadow.strength);
            scene_shader->setFloat("u_ShadowBiasConst", packet.shadow.biasConstant);
            scene_shader->setFloat("u_ShadowBiasSlope", packet.shadow.biasSlope);
            if (has_shadow_cascade && context.shadow_cascade_framebuffers)
            {
                for (uint32_t cascade_index = 0; cascade_index < packet.shadow.cascadeCount; ++cascade_index)
                {
                    const std::shared_ptr<Framebuffer>& shadow_fb = (*context.shadow_cascade_framebuffers)[cascade_index];
                    if (shadow_fb)
                        shadow_fb->bindDepthAttachmentTexture(RenderBindings::kSceneShadowMapSlot + cascade_index);
                }
            }

            auto draw_queue = [&](const std::vector<RenderDrawItem>& items)
            {
                for (const auto& item : items)
                {
                    if (!item.meshGPU || !item.materialGPU || item.indexCount == 0)
                        continue;

                    scene_shader->setMat4("u_Model", item.model);
                    scene_shader->setVec4("u_TintColor", item.tint);
                    scene_shader->setUInt("u_EntityID", encodeEntityID(item.entityID));
                    item.materialGPU->bind(*scene_shader);

                    item.meshGPU->vao->bind();
                    RenderCommand::drawIndexed(item.indexCount, item.indexOffset);
                }
            };

            RenderCommand::setBlendEnabled(false);
            RenderCommand::setDepthTestEnabled(true);
            RenderCommand::setDepthWriteEnabled(true);
            RenderCommand::setDepthCompareFunc(DepthCompareFunc::Less);
            RenderCommand::setCullEnabled(true);
            draw_queue(packet.opaque_items);

            if (!packet.transparent_items.empty())
            {
                RenderCommand::setBlendEnabled(true);
                RenderCommand::setDepthTestEnabled(true);
                RenderCommand::setDepthWriteEnabled(false);
                RenderCommand::setCullEnabled(true);
                draw_queue(packet.transparent_items);
                RenderCommand::setDepthWriteEnabled(true);
                RenderCommand::setBlendEnabled(false);
            }
        }

        Renderer::endFrame();
        framebuffer->unbind();
    }
} // namespace Hybrid
