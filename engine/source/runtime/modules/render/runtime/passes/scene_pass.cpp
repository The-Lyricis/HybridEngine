#include "scene_pass.h"

#include "runtime/modules/render/public/framebuffer.h"
#include "runtime/modules/render/public/render_command.h"
#include "runtime/modules/render/public/renderer.h"
#include "runtime/modules/render/public/shader.h"
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

            draw_queue(packet.opaque_items);
            draw_queue(packet.transparent_items);
        }

        Renderer::endFrame();
        framebuffer->unbind();
    }
} // namespace Hybrid
