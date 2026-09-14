#include "selection_mask_pass.h"

#include <cstddef>
#include <string>
#include <unordered_set>

#include "runtime/core/base/macro.h"
#include "runtime/modules/asset/mesh.h"
#include "runtime/modules/render/rhi/render_device.h"
#include "runtime/modules/render/runtime/mesh_gpu.h"
#include "runtime/modules/render/runtime/pipeline/render_graph_resources.h"
#include "runtime/modules/render/runtime/render_bindings.h"
#include "runtime/modules/render/runtime/render_shaders.h"
#include "runtime/modules/render/runtime/rhi_scene_blocks.h"

namespace Hybrid
{
    namespace
    {
        constexpr const char* kLogTag = "[SelectionMaskPass]";
        std::vector<uint8_t> code(const std::string& source) { return {source.begin(), source.end()}; }
    }

    SelectionMaskPass::~SelectionMaskPass() { shutdown(); }

    void SelectionMaskPass::shutdown()
    {
        if (!m_device) return;
        if (m_pipeline) (void)m_device->destroyPipeline(m_pipeline);
        if (m_vertex_shader) (void)m_device->destroyShaderModule(m_vertex_shader);
        if (m_fragment_shader) (void)m_device->destroyShaderModule(m_fragment_shader);
        for (const auto& buffers : m_draw_buffers)
        {
            if (buffers.draw) (void)m_device->destroyBuffer(buffers.draw);
            if (buffers.material) (void)m_device->destroyBuffer(buffers.material);
        }
        m_draw_buffers.clear();
        m_pipeline = {}; m_vertex_shader = {}; m_fragment_shader = {};
        m_shader_revision = 0; m_device = nullptr;
    }

    bool SelectionMaskPass::ensurePipeline(IRenderDevice& device, ShaderLibrary& shaders)
    {
        if (m_device && m_device != &device) shutdown();
        m_device = &device;
        ShaderLibrary::ShaderSourceBundle sources{};
        if (!shaders.getSources(std::string(RenderShaders::kSelectionMask.name), sources)) return false;
        if (m_pipeline && m_shader_revision == sources.revision) return true;

        ShaderModuleDesc vs{}; vs.stage = RhiShaderStage::Vertex; vs.code = code(sources.vertex); vs.debug_name = "SelectionMask.Vertex";
        const auto vertex = device.createShaderModule(vs);
        if (!vertex) { HBD_CORE_ERROR("{} vertex_shader_failed reason={}", kLogTag, vertex.error.message); return false; }
        ShaderModuleDesc fs{}; fs.stage = RhiShaderStage::Fragment; fs.code = code(sources.fragment); fs.debug_name = "SelectionMask.Fragment";
        const auto fragment = device.createShaderModule(fs);
        if (!fragment) { (void)device.destroyShaderModule(vertex.value); HBD_CORE_ERROR("{} fragment_shader_failed reason={}", kLogTag, fragment.error.message); return false; }

        GraphicsPipelineDesc desc{};
        desc.vertex_shader = vertex.value; desc.fragment_shader = fragment.value;
        desc.vertex_stride = sizeof(MeshVertex);
        desc.vertex_attributes = {
            {0, static_cast<uint32_t>(offsetof(MeshVertex, position)), 3},
            {2, static_cast<uint32_t>(offsetof(MeshVertex, uv)), 2},
        };
        desc.uniform_buffer_bindings = {
            {{RenderBindings::kFrameSet, RenderBindings::kFrameBinding}, "FrameBlock"},
            {{RenderBindings::kSceneMaterialSet, RenderBindings::kSceneMaterialBinding}, RenderBindings::kSceneMaterialBlockName},
            {{RenderBindings::kSceneDrawSet, RenderBindings::kSceneDrawBinding}, RenderBindings::kSceneDrawBlockName},
        };
        desc.texture_bindings = {
            {{RenderBindings::kSceneMaterialSet, RenderBindings::kSceneBaseColorBinding},
             RenderBindings::kSceneBaseColorTextureUniform},
        };
        desc.topology = RhiPrimitiveTopology::Triangles; desc.cull_mode = RhiCullMode::Back;
        desc.depth_test = true; desc.depth_write = true; desc.blend_enabled = false;
        desc.color_format = RhiFormat::R8Unorm; desc.depth_format = RhiFormat::Depth32Float;
        desc.debug_name = "SelectionMask.Pipeline";
        const auto pipeline = device.createGraphicsPipeline(desc);
        if (!pipeline)
        {
            (void)device.destroyShaderModule(vertex.value); (void)device.destroyShaderModule(fragment.value);
            HBD_CORE_ERROR("{} pipeline_failed reason={}", kLogTag, pipeline.error.message);
            return false;
        }
        if (m_pipeline) (void)device.destroyPipeline(m_pipeline);
        if (m_vertex_shader) (void)device.destroyShaderModule(m_vertex_shader);
        if (m_fragment_shader) (void)device.destroyShaderModule(m_fragment_shader);
        m_vertex_shader = vertex.value; m_fragment_shader = fragment.value;
        m_pipeline = pipeline.value; m_shader_revision = sources.revision;
        HBD_CORE_INFO("{} pipeline_ready shader_revision={}", kLogTag, m_shader_revision);
        return true;
    }

    bool SelectionMaskPass::ensureBuffers(IRenderDevice& device, size_t count)
    {
        while (m_draw_buffers.size() < count)
        {
            BufferDesc desc{}; desc.usage = RhiBufferUsage::Uniform; desc.memory = RhiMemoryUsage::CPUToGPU;
            desc.size = sizeof(SceneDrawGPU); desc.debug_name = "SelectionMask.Draw";
            const auto draw = device.createBuffer(desc);
            if (!draw) return false;
            desc.size = sizeof(SceneMaterialGPU); desc.debug_name = "SelectionMask.Material";
            const auto material = device.createBuffer(desc);
            if (!material) { (void)device.destroyBuffer(draw.value); return false; }
            m_draw_buffers.push_back({draw.value, material.value});
        }
        return true;
    }

    void SelectionMaskPass::execute(RenderContext& context)
    {
        if (!context.packet || !context.editor_selection || context.editor_selection->selected_entities.empty() ||
            !context.device || !context.shader_library || !context.graph_resources || !context.frame_uniform_buffer) return;
        IRenderDevice& device = *context.device;
        const auto mask = context.graph_resources->texture("SelectionMask");
        const auto depth = context.graph_resources->texture("SelectionDepth");
        if (!mask || !depth || !ensurePipeline(device, *context.shader_library)) return;
        const auto size = device.textureDesc(mask.value);
        if (!size) return;

        const std::unordered_set<uint32_t> selected(context.editor_selection->selected_entities.begin(),
                                                    context.editor_selection->selected_entities.end());
        std::vector<const RenderDrawItem*> items;
        const auto collect = [&items, &selected](const auto& queue)
        {
            for (const RenderDrawItem& item : queue)
                if (selected.find(item.entityID) != selected.end() && item.materialGPU &&
                    item.meshGPU && item.meshGPU->rhi_vertex_buffer &&
                    item.meshGPU->rhi_index_buffer && item.indexCount > 0) items.push_back(&item);
        };
        collect(context.packet->opaque_items); collect(context.packet->transparent_items);
        if (!ensureBuffers(device, items.size())) return;
        for (size_t index = 0; index < items.size(); ++index)
        {
            SceneDrawGPU draw{}; draw.model = items[index]->model; draw.tint = items[index]->tint;
            draw.ids.x = items[index]->entityID + 1u;
            const SceneMaterialGPU material = BuildSceneMaterialGPU(items[index]->materialGPU);
            if (!device.updateBuffer(m_draw_buffers[index].draw, 0, &draw, sizeof(draw)) ||
                !device.updateBuffer(m_draw_buffers[index].material, 0, &material, sizeof(material))) return;
        }

        auto commands = device.createCommandList();
        const auto check = [](const RhiStatus& status, const char* stage)
        {
            if (status) return true;
            HBD_CORE_ERROR("{} command_failed stage={} reason={}", kLogTag, stage, status.error.message);
            return false;
        };
        if (!commands || !check(commands->begin(), "begin")) return;
        RenderPassDesc pass{}; pass.colors.push_back({mask.value});
        pass.has_depth = true; pass.depth.texture = depth.value; pass.debug_name = "SelectionMask";
        if (!check(commands->beginRenderPass(pass), "render_pass") ||
            !check(commands->setViewport({0, 0, static_cast<float>(size.value.width), static_cast<float>(size.value.height)}), "viewport") ||
            !check(commands->bindPipeline(m_pipeline), "pipeline") ||
            !check(commands->bindUniformBuffer(context.frame_uniform_buffer, {RenderBindings::kFrameSet, RenderBindings::kFrameBinding}), "frame")) return;
        for (size_t index = 0; index < items.size(); ++index)
        {
            if (!items[index]->materialGPU->bindBaseColor(*commands) ||
                !check(commands->bindUniformBuffer(m_draw_buffers[index].material, {RenderBindings::kSceneMaterialSet, RenderBindings::kSceneMaterialBinding}), "material") ||
                !check(commands->bindUniformBuffer(m_draw_buffers[index].draw, {RenderBindings::kSceneDrawSet, RenderBindings::kSceneDrawBinding}), "draw") ||
                !check(commands->bindVertexBuffer(items[index]->meshGPU->rhi_vertex_buffer), "vertices") ||
                !check(commands->bindIndexBuffer(items[index]->meshGPU->rhi_index_buffer), "indices") ||
                !check(commands->drawIndexed(items[index]->indexCount, items[index]->indexOffset), "draw_indexed")) return;
        }
        if (!check(commands->endRenderPass(), "end_render_pass") ||
            !check(commands->end(), "end")) return;
        (void)check(device.submit(*commands), "submit");
    }
} // namespace Hybrid
