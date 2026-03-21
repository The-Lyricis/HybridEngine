#include "selection_mask_pass.h"

#include <unordered_set>

#include "runtime/modules/render/public/framebuffer.h"
#include "runtime/modules/render/public/render_command.h"
#include "runtime/modules/render/public/shader.h"
#include "runtime/modules/render/public/vertex_array.h"
#include "runtime/modules/render/runtime/render_uniforms.h"

namespace Hybrid
{
    void SelectionMaskPass::execute(RenderContext& context)
    {
        const RenderPacket& packet = *context.packet;
        const std::shared_ptr<Framebuffer>& framebuffer = context.selection_framebuffer;
        const EditorSelectionState* selection = context.editor_selection;
        auto asset_manager = context.asset_manager;

        if (!framebuffer || !selection || selection->selected_entities.empty() ||
            !asset_manager || !context.resolve_mesh_gpu || context.shader_library == nullptr)
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

        const std::unordered_set<uint32_t> selected_entities(selection->selected_entities.begin(),
                                                             selection->selected_entities.end());

        for (const auto& item : packet.items)
        {
            if (selected_entities.find(item.entityID) == selected_entities.end() || item.meshId.value == 0)
                continue;

            std::shared_ptr<Mesh> cpu_mesh = asset_manager->loadSync<Mesh>(item.meshId);
            if (!cpu_mesh)
                continue;

            MeshGPU* mesh_gpu = context.resolve_mesh_gpu(item.meshId, cpu_mesh);
            if (!mesh_gpu)
                continue;

            shader->setMat4("u_Model", item.model);
            mesh_gpu->vao->bind();

            for (const auto& submesh : mesh_gpu->submeshes)
                RenderCommand::drawIndexed(submesh.index_count, submesh.index_offset);
        }

        framebuffer->unbind();
    }
} // namespace Hybrid
