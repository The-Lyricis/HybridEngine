#include "selection_mask_pass.h"

#include <unordered_set>

#include "runtime/modules/render/public/framebuffer.h"
#include "runtime/modules/render/public/render_command.h"
#include "runtime/modules/render/public/shader.h"
#include "runtime/modules/render/public/vertex_array.h"
#include "runtime/modules/render/runtime/render_bindings.h"
#include "runtime/modules/render/runtime/render_uniforms.h"

namespace Hybrid
{
    void SelectionMaskPass::execute(RenderContext& context)
    {
        const RenderPacket& packet = *context.packet;
        const std::shared_ptr<Framebuffer>& framebuffer = context.selection_framebuffer;
        const EditorSelectionState* selection = context.editor_selection;

        if (!framebuffer || !selection || selection->selected_entities.empty() || context.shader_library == nullptr)
        {
            return;
        }

        auto shader = context.shader_library->get("SelectionMask");
        if (!shader)
            return;

        framebuffer->bind();
        RenderCommand::setViewport(0, 0, framebuffer->getWidth(), framebuffer->getHeight());
        RenderCommand::setBlendEnabled(false);
        RenderCommand::setDepthTestEnabled(true);
        RenderCommand::setDepthWriteEnabled(true);
        RenderCommand::setCullEnabled(true);
        RenderCommand::setClearColor(glm::vec4(0.0f));
        RenderCommand::setClearDepth(1.0f);
        RenderCommand::clear();

        shader->bind();
        shader->setUniformBlockBinding(RenderUniforms::kFrameBlockName,
                                       RenderUniforms::kFrameUBOBinding);
        shader->setInt(RenderBindings::kSceneAlbedoUniform, RenderBindings::kSceneAlbedoSlot);

        const std::unordered_set<uint32_t> selected_entities(selection->selected_entities.begin(),
                                                             selection->selected_entities.end());

        auto draw_selected_items = [&](const std::vector<RenderDrawItem>& items)
        {
            for (const auto& item : items)
            {
                if (selected_entities.find(item.entityID) == selected_entities.end() ||
                    !item.meshGPU || !item.materialGPU || item.indexCount == 0)
                    continue;

                shader->setMat4("u_Model", item.model);
                shader->setVec4("u_TintColor", item.tint);
                item.materialGPU->bind(*shader);
                item.meshGPU->vao->bind();
                RenderCommand::drawIndexed(item.indexCount, item.indexOffset);
            }
        };

        draw_selected_items(packet.opaque_items);
        draw_selected_items(packet.transparent_items);

        framebuffer->unbind();
    }
} // namespace Hybrid
