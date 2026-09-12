#include "selection_overlay_pass.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "runtime/core/base/macro.h"
#include "runtime/modules/render/public/framebuffer.h"
#include "runtime/modules/render/rhi/render_device.h"
#include "runtime/modules/render/runtime/pipeline/render_graph_resources.h"
#include "runtime/modules/render/runtime/render_bindings.h"
#include "runtime/modules/render/runtime/render_shaders.h"
#include "runtime/modules/render/runtime/render_targets.h"
#include "runtime/modules/render/runtime/shader_library.h"

namespace Hybrid
{
    namespace
    {
        constexpr const char* kLogTag = "[SelectionOverlayPass]";
        struct FullscreenVertex { float position[2]; float uv[2]; };
        struct alignas(16) SettingsGPU
        {
            float visible[4];
            float occluded[4];
            float fill[4];
            float metrics[4]; // texel width, texel height, depth epsilon, padding
        };
        static_assert(sizeof(SettingsGPU) == 64);
        constexpr std::array<FullscreenVertex, 4> kVertices = {
            FullscreenVertex{{-1.0f, -1.0f}, {0.0f, 0.0f}},
            FullscreenVertex{{ 1.0f, -1.0f}, {1.0f, 0.0f}},
            FullscreenVertex{{ 1.0f,  1.0f}, {1.0f, 1.0f}},
            FullscreenVertex{{-1.0f,  1.0f}, {0.0f, 1.0f}},
        };
        constexpr std::array<uint32_t, 6> kIndices = {0, 1, 2, 2, 3, 0};
        std::vector<uint8_t> shaderCode(const std::string& source) { return {source.begin(), source.end()}; }
        void copyColor(float (&out)[4], const glm::vec4& in)
        { out[0] = in.x; out[1] = in.y; out[2] = in.z; out[3] = in.w; }
    }

    SelectionOverlayPass::~SelectionOverlayPass() { shutdown(); }

    void SelectionOverlayPass::releasePipeline()
    {
        if (!m_Device) return;
        if (m_Pipeline) (void)m_Device->destroyPipeline(m_Pipeline);
        if (m_VertexShader) (void)m_Device->destroyShaderModule(m_VertexShader);
        if (m_FragmentShader) (void)m_Device->destroyShaderModule(m_FragmentShader);
        m_Pipeline = {}; m_VertexShader = {}; m_FragmentShader = {}; m_ShaderRevision = 0;
    }

    void SelectionOverlayPass::shutdown()
    {
        if (!m_Device) return;
        releasePipeline();
        if (m_Sampler) (void)m_Device->destroySampler(m_Sampler);
        if (m_SettingsBuffer) (void)m_Device->destroyBuffer(m_SettingsBuffer);
        if (m_IndexBuffer) (void)m_Device->destroyBuffer(m_IndexBuffer);
        if (m_VertexBuffer) (void)m_Device->destroyBuffer(m_VertexBuffer);
        m_Sampler = {}; m_SettingsBuffer = {}; m_IndexBuffer = {}; m_VertexBuffer = {}; m_Device = nullptr;
    }

    bool SelectionOverlayPass::ensureStaticResources(IRenderDevice& device)
    {
        if (m_Device && m_Device != &device) shutdown();
        m_Device = &device;
        const auto create = [&device](BufferHandle& handle, const void* data, size_t size,
                                      RhiBufferUsage usage, const char* name)
        {
            if (handle) return true;
            BufferDesc desc{}; desc.size = size; desc.usage = usage; desc.memory = RhiMemoryUsage::CPUToGPU; desc.debug_name = name;
            const auto result = device.createBuffer(desc, data, data ? size : 0);
            if (!result) { HBD_CORE_ERROR("{} buffer_create_failed name={} reason={}", kLogTag, name, result.error.message); return false; }
            handle = result.value;
            return true;
        };
        if (!create(m_VertexBuffer, kVertices.data(), sizeof(kVertices), RhiBufferUsage::Vertex, "SelectionOverlay.Vertices") ||
            !create(m_IndexBuffer, kIndices.data(), sizeof(kIndices), RhiBufferUsage::Index, "SelectionOverlay.Indices") ||
            !create(m_SettingsBuffer, nullptr, sizeof(SettingsGPU), RhiBufferUsage::Uniform, "SelectionOverlay.Settings")) return false;
        if (!m_Sampler)
        {
            SamplerDesc desc{}; desc.linear_filter = true; desc.clamp_to_edge = true; desc.debug_name = "SelectionOverlay.LinearClamp";
            const auto result = device.createSampler(desc);
            if (!result) { HBD_CORE_ERROR("{} sampler_create_failed reason={}", kLogTag, result.error.message); return false; }
            m_Sampler = result.value;
        }
        return true;
    }

    bool SelectionOverlayPass::ensurePipeline(IRenderDevice& device, ShaderLibrary& shaders)
    {
        ShaderLibrary::ShaderSourceBundle sources{};
        if (!shaders.getSources(std::string(RenderShaders::kSelectionOverlay.name), sources))
        { HBD_CORE_ERROR("{} shader_sources_unavailable", kLogTag); return false; }
        if (m_Pipeline && m_ShaderRevision == sources.revision) return true;
        ShaderModuleDesc vertex{}; vertex.stage = RhiShaderStage::Vertex; vertex.code = shaderCode(sources.vertex); vertex.debug_name = "SelectionOverlay.Vertex";
        const auto vertex_result = device.createShaderModule(vertex);
        if (!vertex_result) { HBD_CORE_ERROR("{} vertex_shader_create_failed reason={}", kLogTag, vertex_result.error.message); return false; }
        ShaderModuleDesc fragment{}; fragment.stage = RhiShaderStage::Fragment; fragment.code = shaderCode(sources.fragment); fragment.debug_name = "SelectionOverlay.Fragment";
        const auto fragment_result = device.createShaderModule(fragment);
        if (!fragment_result) { (void)device.destroyShaderModule(vertex_result.value); HBD_CORE_ERROR("{} fragment_shader_create_failed reason={}", kLogTag, fragment_result.error.message); return false; }
        GraphicsPipelineDesc desc{};
        desc.vertex_shader = vertex_result.value; desc.fragment_shader = fragment_result.value;
        desc.vertex_stride = sizeof(FullscreenVertex); desc.vertex_attributes = {{0, 0, 2}, {1, sizeof(float) * 2u, 2}};
        desc.texture_bindings = {
            {{RenderBindings::kSelectionOverlayTextureSet, RenderBindings::kSelectionOverlaySceneColorSlot}, RenderBindings::kSelectionOverlaySceneColorUniform},
            {{RenderBindings::kSelectionOverlayTextureSet, RenderBindings::kSelectionOverlaySceneDepthSlot}, RenderBindings::kSelectionOverlaySceneDepthUniform},
            {{RenderBindings::kSelectionOverlayTextureSet, RenderBindings::kSelectionOverlayMaskSlot}, RenderBindings::kSelectionOverlayMaskUniform},
            {{RenderBindings::kSelectionOverlayTextureSet, RenderBindings::kSelectionOverlaySelectedDepthSlot}, RenderBindings::kSelectionOverlaySelectedDepthUniform},
        };
        desc.uniform_buffer_bindings = {
            {{RenderBindings::kSelectionOverlaySettingsSet, RenderBindings::kSelectionOverlaySettingsBinding}, RenderBindings::kSelectionOverlaySettingsBlockName},
        };
        desc.topology = RhiPrimitiveTopology::Triangles; desc.cull_mode = RhiCullMode::None;
        desc.depth_test = false; desc.depth_write = false; desc.blend_enabled = false;
        desc.color_format = RhiFormat::RGBA8Unorm; desc.depth_format = RhiFormat::Unknown; desc.debug_name = "SelectionOverlay.Pipeline";
        const auto pipeline = device.createGraphicsPipeline(desc);
        if (!pipeline) { (void)device.destroyShaderModule(vertex_result.value); (void)device.destroyShaderModule(fragment_result.value); HBD_CORE_ERROR("{} pipeline_create_failed reason={}", kLogTag, pipeline.error.message); return false; }
        releasePipeline(); m_VertexShader = vertex_result.value; m_FragmentShader = fragment_result.value; m_Pipeline = pipeline.value; m_ShaderRevision = sources.revision;
        HBD_CORE_INFO("{} pipeline_ready shader_revision={}", kLogTag, m_ShaderRevision);
        return true;
    }

    void SelectionOverlayPass::execute(RenderContext& context)
    {
        const auto& scene = context.scene_framebuffer;
        const RenderSelectionState* selection = context.editor_selection;
        if (!scene || !selection || selection->selected_entities.empty() || !context.selection_overlay_style || !context.shader_library || !context.device || !context.graph_resources) return;
        const auto input = context.graph_resources->texture("SelectionOverlayInput");
        const auto scene_depth = context.graph_resources->texture("SceneDepth");
        const auto mask = context.graph_resources->texture("SelectionMask");
        const auto selected_depth = context.graph_resources->texture("SelectionDepth");
        const TextureViewHandle output = scene->getColorAttachmentView(RenderTargets::kSceneColorAttachment);
        if (!input || !scene_depth || !mask || !selected_depth || !output) { HBD_CORE_ERROR("{} graph_resource_unavailable", kLogTag); return; }
        IRenderDevice& device = *context.device;
        if (!ensureStaticResources(device) || !ensurePipeline(device, *context.shader_library)) return;
        const uint32_t width = scene->getWidth(), height = scene->getHeight();
        SettingsGPU settings{};
        copyColor(settings.visible, context.selection_overlay_style->visible_outline_color);
        copyColor(settings.occluded, context.selection_overlay_style->occluded_outline_color);
        copyColor(settings.fill, context.selection_overlay_style->fill_color);
        settings.metrics[0] = width ? 1.0f / static_cast<float>(width) : 0.0f;
        settings.metrics[1] = height ? 1.0f / static_cast<float>(height) : 0.0f;
        settings.metrics[2] = context.selection_overlay_style->depth_epsilon;
        if (const RhiStatus status = device.updateBuffer(m_SettingsBuffer, 0, &settings, sizeof(settings)); !status) { HBD_CORE_ERROR("{} settings_upload_failed reason={}", kLogTag, status.error.message); return; }
        auto commands = device.createCommandList();
        const auto check = [](const RhiStatus& status, const char* stage) { if (status) return true; HBD_CORE_ERROR("{} command_failed stage={} reason={}", kLogTag, stage, status.error.message); return false; };
        if (!commands || !check(commands->begin(), "begin") || !check(commands->copyTexture(output, input.value), "copy_input")) return;
        RenderPassDesc pass{}; pass.colors.push_back({output, {}, false, true}); pass.debug_name = "SelectionOverlay";
        if (!check(commands->beginRenderPass(pass), "begin_render_pass") || !check(commands->setViewport({0, 0, static_cast<float>(width), static_cast<float>(height)}), "viewport") ||
            !check(commands->bindPipeline(m_Pipeline), "pipeline") ||
            !check(commands->bindTexture(input.value, m_Sampler, {RenderBindings::kSelectionOverlayTextureSet, RenderBindings::kSelectionOverlaySceneColorSlot}), "scene_color") ||
            !check(commands->bindTexture(scene_depth.value, m_Sampler, {RenderBindings::kSelectionOverlayTextureSet, RenderBindings::kSelectionOverlaySceneDepthSlot}), "scene_depth") ||
            !check(commands->bindTexture(mask.value, m_Sampler, {RenderBindings::kSelectionOverlayTextureSet, RenderBindings::kSelectionOverlayMaskSlot}), "mask") ||
            !check(commands->bindTexture(selected_depth.value, m_Sampler, {RenderBindings::kSelectionOverlayTextureSet, RenderBindings::kSelectionOverlaySelectedDepthSlot}), "selected_depth") ||
            !check(commands->bindUniformBuffer(m_SettingsBuffer, {RenderBindings::kSelectionOverlaySettingsSet, RenderBindings::kSelectionOverlaySettingsBinding}), "settings") ||
            !check(commands->bindVertexBuffer(m_VertexBuffer), "vertices") || !check(commands->bindIndexBuffer(m_IndexBuffer), "indices") ||
            !check(commands->drawIndexed(static_cast<uint32_t>(kIndices.size())), "draw") || !check(commands->endRenderPass(), "end_render_pass") || !check(commands->end(), "end") || !check(device.submit(*commands), "submit")) return;
    }
} // namespace Hybrid
