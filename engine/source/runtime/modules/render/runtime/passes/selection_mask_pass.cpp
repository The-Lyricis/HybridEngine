#include "selection_mask_pass.h"

#include <unordered_set>

#include <glad/gl.h>

#include "runtime/modules/render/public/framebuffer.h"
#include "runtime/modules/render/public/render_command.h"
#include "runtime/modules/render/public/shader.h"
#include "runtime/modules/render/public/vertex_array.h"

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
        glDisable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
        glEnable(GL_CULL_FACE);
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClearDepth(1.0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        shader->bind();
        shader->setMat4("u_ViewProjection", packet.frame.viewProj);

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
