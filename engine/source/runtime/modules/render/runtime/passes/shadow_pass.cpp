#include "shadow_pass.h"

#include <algorithm>
#include <cstddef>
#include <string>

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
        constexpr const char* kLogTag = "[ShadowPass]";
        constexpr std::array<const char*, kMaxDirectionalShadowCascades> kDepthNames{
            "ShadowDepth", "ShadowDepth1", "ShadowDepth2", "ShadowDepth3"
        };
        std::vector<uint8_t> code(const std::string& source) { return {source.begin(), source.end()}; }
    }

    ShadowPass::~ShadowPass() { shutdown(); }

    void ShadowPass::shutdown()
    {
        if (!m_device) return;
        if (m_pipeline) (void)m_device->destroyPipeline(m_pipeline);
        if (m_vertex_shader) (void)m_device->destroyShaderModule(m_vertex_shader);
        if (m_fragment_shader) (void)m_device->destroyShaderModule(m_fragment_shader);
        for (BufferHandle& view : m_view_buffers)
        {
            if (view) (void)m_device->destroyBuffer(view);
            view = {};
        }
        for (const auto& buffers : m_draw_buffers)
        {
            if (buffers.draw) (void)m_device->destroyBuffer(buffers.draw);
            if (buffers.material) (void)m_device->destroyBuffer(buffers.material);
        }
        m_draw_buffers.clear(); m_pipeline = {}; m_vertex_shader = {}; m_fragment_shader = {};
        m_shader_revision = 0; m_device = nullptr;
    }

    bool ShadowPass::ensurePipeline(IRenderDevice& device, ShaderLibrary& shaders)
    {
        if (m_device && m_device != &device) shutdown();
        m_device = &device;
        ShaderLibrary::ShaderSourceBundle sources{};
        if (!shaders.getSources(std::string(RenderShaders::kShadowDepth.name), sources)) return false;
        if (m_pipeline && m_shader_revision == sources.revision) return true;
        ShaderModuleDesc vs{}; vs.stage = RhiShaderStage::Vertex; vs.code = code(sources.vertex); vs.debug_name = "Shadow.Vertex";
        const auto vertex = device.createShaderModule(vs);
        if (!vertex) { HBD_CORE_ERROR("{} vertex_shader_failed reason={}", kLogTag, vertex.error.message); return false; }
        ShaderModuleDesc fs{}; fs.stage = RhiShaderStage::Fragment; fs.code = code(sources.fragment); fs.debug_name = "Shadow.Fragment";
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
            {{RenderBindings::kShadowViewSet, RenderBindings::kShadowViewBinding}, RenderBindings::kShadowViewBlockName},
            {{RenderBindings::kSceneMaterialSet, RenderBindings::kSceneMaterialBinding}, RenderBindings::kSceneMaterialBlockName},
            {{RenderBindings::kSceneDrawSet, RenderBindings::kSceneDrawBinding}, RenderBindings::kSceneDrawBlockName},
        };
        desc.topology = RhiPrimitiveTopology::Triangles; desc.cull_mode = RhiCullMode::Back;
        desc.depth_test = true; desc.depth_write = true; desc.blend_enabled = false;
        desc.color_format = RhiFormat::Unknown; desc.depth_format = RhiFormat::Depth32Float;
        desc.debug_name = "Shadow.Pipeline";
        const auto pipeline = device.createGraphicsPipeline(desc);
        if (!pipeline)
        {
            (void)device.destroyShaderModule(vertex.value); (void)device.destroyShaderModule(fragment.value);
            HBD_CORE_ERROR("{} pipeline_failed reason={}", kLogTag, pipeline.error.message); return false;
        }
        if (m_pipeline) (void)device.destroyPipeline(m_pipeline);
        if (m_vertex_shader) (void)device.destroyShaderModule(m_vertex_shader);
        if (m_fragment_shader) (void)device.destroyShaderModule(m_fragment_shader);
        m_vertex_shader = vertex.value; m_fragment_shader = fragment.value;
        m_pipeline = pipeline.value; m_shader_revision = sources.revision;
        HBD_CORE_INFO("{} pipeline_ready shader_revision={}", kLogTag, m_shader_revision);
        return true;
    }

    bool ShadowPass::ensureBuffers(IRenderDevice& device, size_t count)
    {
        BufferDesc desc{}; desc.usage = RhiBufferUsage::Uniform; desc.memory = RhiMemoryUsage::CPUToGPU;
        desc.size = sizeof(glm::mat4); desc.debug_name = "Shadow.View";
        for (BufferHandle& view : m_view_buffers)
        {
            if (view) continue;
            const auto result = device.createBuffer(desc);
            if (!result) return false;
            view = result.value;
        }
        while (m_draw_buffers.size() < count)
        {
            desc.size = sizeof(SceneDrawGPU); desc.debug_name = "Shadow.Draw";
            const auto draw = device.createBuffer(desc);
            if (!draw) return false;
            desc.size = sizeof(SceneMaterialGPU); desc.debug_name = "Shadow.Material";
            const auto material = device.createBuffer(desc);
            if (!material) { (void)device.destroyBuffer(draw.value); return false; }
            m_draw_buffers.push_back({draw.value, material.value});
        }
        return true;
    }

    void ShadowPass::execute(RenderContext& context)
    {
        if (!context.packet || !context.device || !context.shader_library || !context.graph_resources) return;
        const RenderPacket& packet = *context.packet;
        if (!packet.shadow.enabled || packet.shadow.cascadeCount == 0) return;
        IRenderDevice& device = *context.device;
        if (!ensurePipeline(device, *context.shader_library) || !ensureBuffers(device, packet.shadow_caster_items.size())) return;

        for (size_t index = 0; index < packet.shadow_caster_items.size(); ++index)
        {
            const RenderDrawItem& item = packet.shadow_caster_items[index];
            SceneDrawGPU draw{}; draw.model = item.model; draw.tint = item.tint; draw.ids.x = item.entityID + 1u;
            const SceneMaterialGPU material = BuildSceneMaterialGPU(item.materialGPU);
            if (!device.updateBuffer(m_draw_buffers[index].draw, 0, &draw, sizeof(draw)) ||
                !device.updateBuffer(m_draw_buffers[index].material, 0, &material, sizeof(material))) return;
        }

        const auto check = [](const RhiStatus& status, const char* stage)
        {
            if (status) return true;
            HBD_CORE_ERROR("{} command_failed stage={} reason={}", kLogTag, stage, status.error.message);
            return false;
        };
        for (uint32_t cascade = 0; cascade < std::min(packet.shadow.cascadeCount, kMaxDirectionalShadowCascades); ++cascade)
        {
            if (!packet.shadow.cascades[cascade].valid) continue;
            const auto depth = context.graph_resources->texture(kDepthNames[cascade]);
            if (!depth) { HBD_CORE_ERROR("{} depth_unavailable cascade={} reason={}", kLogTag, cascade, depth.error.message); return; }
            const auto size = device.textureDesc(depth.value);
            if (!size) return;
            const glm::mat4 view_projection = packet.shadow.cascades[cascade].lightViewProjection;
            if (!device.updateBuffer(m_view_buffers[cascade], 0, &view_projection, sizeof(view_projection))) return;
            auto commands = device.createCommandList();
            if (!commands || !check(commands->begin(), "begin")) return;
            RenderPassDesc pass{}; pass.has_depth = true; pass.depth.texture = depth.value; pass.debug_name = "Shadow";
            if (!check(commands->beginRenderPass(pass), "render_pass") ||
                !check(commands->setViewport({0, 0, static_cast<float>(size.value.width), static_cast<float>(size.value.height)}), "viewport") ||
                !check(commands->bindPipeline(m_pipeline), "pipeline") ||
                !check(commands->bindUniformBuffer(m_view_buffers[cascade], {RenderBindings::kShadowViewSet, RenderBindings::kShadowViewBinding}), "view")) return;
            for (size_t index = 0; index < packet.shadow_caster_items.size(); ++index)
            {
                const RenderDrawItem& item = packet.shadow_caster_items[index];
                if (!item.meshGPU || !item.meshGPU->rhi_vertex_buffer || !item.meshGPU->rhi_index_buffer || item.indexCount == 0) continue;
                if (!check(commands->bindUniformBuffer(m_draw_buffers[index].material, {RenderBindings::kSceneMaterialSet, RenderBindings::kSceneMaterialBinding}), "material") ||
                    !check(commands->bindUniformBuffer(m_draw_buffers[index].draw, {RenderBindings::kSceneDrawSet, RenderBindings::kSceneDrawBinding}), "draw") ||
                    !check(commands->bindVertexBuffer(item.meshGPU->rhi_vertex_buffer), "vertices") ||
                    !check(commands->bindIndexBuffer(item.meshGPU->rhi_index_buffer), "indices") ||
                    !check(commands->drawIndexed(item.indexCount, item.indexOffset), "draw_indexed")) return;
            }
            if (!check(commands->endRenderPass(), "end_render_pass") ||
                !check(commands->end(), "end") || !check(device.submit(*commands), "submit")) return;
        }
    }
} // namespace Hybrid
