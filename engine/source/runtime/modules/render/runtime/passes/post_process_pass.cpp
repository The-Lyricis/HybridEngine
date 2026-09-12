#include "post_process_pass.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "runtime/core/base/macro.h"
#include "runtime/modules/render/public/framebuffer.h"
#include "runtime/modules/render/rhi/render_device.h"
#include "runtime/modules/render/runtime/render_bindings.h"
#include "runtime/modules/render/runtime/pipeline/render_graph_resources.h"
#include "runtime/modules/render/runtime/render_shaders.h"
#include "runtime/modules/render/runtime/render_targets.h"
#include "runtime/modules/render/runtime/shader_library.h"

namespace Hybrid
{
    namespace
    {
        constexpr const char* kPostProcessLogTag = "[PostProcessPass]";

        struct FullscreenVertex
        {
            float position[2];
            float uv[2];
        };

        struct alignas(16) PostProcessSettingsGPU
        {
            int32_t enable_tone_mapping = 0;
            int32_t enable_gamma_correction = 0;
            float exposure = 1.0f;
            float gamma = 2.2f;
        };
        static_assert(sizeof(PostProcessSettingsGPU) == 16);

        constexpr std::array<FullscreenVertex, 4> kFullscreenVertices = {
            FullscreenVertex{{-1.0f, -1.0f}, {0.0f, 0.0f}},
            FullscreenVertex{{ 1.0f, -1.0f}, {1.0f, 0.0f}},
            FullscreenVertex{{ 1.0f,  1.0f}, {1.0f, 1.0f}},
            FullscreenVertex{{-1.0f,  1.0f}, {0.0f, 1.0f}},
        };
        constexpr std::array<uint32_t, 6> kFullscreenIndices = {0, 1, 2, 2, 3, 0};

        std::vector<uint8_t> shaderCode(const std::string& source)
        {
            return std::vector<uint8_t>(source.begin(), source.end());
        }
    } // namespace

    PostProcessPass::~PostProcessPass()
    {
        shutdown();
    }

    void PostProcessPass::releasePipeline()
    {
        if (!m_Device)
            return;
        if (m_Pipeline)
            (void)m_Device->destroyPipeline(m_Pipeline);
        if (m_VertexShader)
            (void)m_Device->destroyShaderModule(m_VertexShader);
        if (m_FragmentShader)
            (void)m_Device->destroyShaderModule(m_FragmentShader);
        m_Pipeline = {};
        m_VertexShader = {};
        m_FragmentShader = {};
        m_ShaderRevision = 0;
    }

    void PostProcessPass::shutdown()
    {
        if (!m_Device)
            return;
        releasePipeline();
        if (m_Sampler)
            (void)m_Device->destroySampler(m_Sampler);
        if (m_SettingsBuffer)
            (void)m_Device->destroyBuffer(m_SettingsBuffer);
        if (m_IndexBuffer)
            (void)m_Device->destroyBuffer(m_IndexBuffer);
        if (m_VertexBuffer)
            (void)m_Device->destroyBuffer(m_VertexBuffer);
        m_Sampler = {};
        m_SettingsBuffer = {};
        m_IndexBuffer = {};
        m_VertexBuffer = {};
        m_Device = nullptr;
    }

    bool PostProcessPass::ensureStaticResources(IRenderDevice& device)
    {
        if (m_Device && m_Device != &device)
            shutdown();
        m_Device = &device;

        if (!m_VertexBuffer)
        {
            BufferDesc desc{};
            desc.size = sizeof(kFullscreenVertices);
            desc.usage = RhiBufferUsage::Vertex;
            desc.memory = RhiMemoryUsage::GPUOnly;
            desc.debug_name = "PostProcess.FullscreenVertices";
            const auto result = device.createBuffer(desc, kFullscreenVertices.data(), desc.size);
            if (!result)
            {
                HBD_CORE_ERROR("{} vertex_buffer_create_failed reason={}", kPostProcessLogTag, result.error.message);
                return false;
            }
            m_VertexBuffer = result.value;
        }

        if (!m_IndexBuffer)
        {
            BufferDesc desc{};
            desc.size = sizeof(kFullscreenIndices);
            desc.usage = RhiBufferUsage::Index;
            desc.memory = RhiMemoryUsage::GPUOnly;
            desc.debug_name = "PostProcess.FullscreenIndices";
            const auto result = device.createBuffer(desc, kFullscreenIndices.data(), desc.size);
            if (!result)
            {
                HBD_CORE_ERROR("{} index_buffer_create_failed reason={}", kPostProcessLogTag, result.error.message);
                return false;
            }
            m_IndexBuffer = result.value;
        }

        if (!m_SettingsBuffer)
        {
            BufferDesc desc{};
            desc.size = sizeof(PostProcessSettingsGPU);
            desc.usage = RhiBufferUsage::Uniform;
            desc.memory = RhiMemoryUsage::CPUToGPU;
            desc.debug_name = "PostProcess.Settings";
            const auto result = device.createBuffer(desc);
            if (!result)
            {
                HBD_CORE_ERROR("{} settings_buffer_create_failed reason={}", kPostProcessLogTag, result.error.message);
                return false;
            }
            m_SettingsBuffer = result.value;
        }

        if (!m_Sampler)
        {
            SamplerDesc desc{};
            desc.linear_filter = true;
            desc.clamp_to_edge = true;
            desc.debug_name = "PostProcess.LinearClamp";
            const auto result = device.createSampler(desc);
            if (!result)
            {
                HBD_CORE_ERROR("{} sampler_create_failed reason={}", kPostProcessLogTag, result.error.message);
                return false;
            }
            m_Sampler = result.value;
        }
        return true;
    }

    bool PostProcessPass::ensurePipeline(IRenderDevice& device, ShaderLibrary& shaders)
    {
        ShaderLibrary::ShaderSourceBundle sources{};
        if (!shaders.getSources(std::string(RenderShaders::kPostProcess.name), sources))
        {
            HBD_CORE_ERROR("{} shader_sources_unavailable", kPostProcessLogTag);
            return false;
        }
        if (m_Pipeline && m_ShaderRevision == sources.revision)
            return true;

        ShaderModuleDesc vertex_desc{};
        vertex_desc.stage = RhiShaderStage::Vertex;
        vertex_desc.code = shaderCode(sources.vertex);
        vertex_desc.debug_name = "PostProcess.Vertex";
        const auto vertex = device.createShaderModule(vertex_desc);
        if (!vertex)
        {
            HBD_CORE_ERROR("{} vertex_shader_create_failed reason={}", kPostProcessLogTag, vertex.error.message);
            return false;
        }

        ShaderModuleDesc fragment_desc{};
        fragment_desc.stage = RhiShaderStage::Fragment;
        fragment_desc.code = shaderCode(sources.fragment);
        fragment_desc.debug_name = "PostProcess.Fragment";
        const auto fragment = device.createShaderModule(fragment_desc);
        if (!fragment)
        {
            (void)device.destroyShaderModule(vertex.value);
            HBD_CORE_ERROR("{} fragment_shader_create_failed reason={}", kPostProcessLogTag, fragment.error.message);
            return false;
        }

        GraphicsPipelineDesc pipeline_desc{};
        pipeline_desc.vertex_shader = vertex.value;
        pipeline_desc.fragment_shader = fragment.value;
        pipeline_desc.vertex_stride = sizeof(FullscreenVertex);
        pipeline_desc.vertex_attributes = {
            {0, 0, 2},
            {1, sizeof(float) * 2u, 2},
        };
        pipeline_desc.texture_bindings = {
            {{RenderBindings::kPostProcessTextureSet,
              RenderBindings::kPostProcessSceneColorSlot},
             RenderBindings::kPostProcessSceneColorUniform},
        };
        pipeline_desc.uniform_buffer_bindings = {
            {{RenderBindings::kPostProcessSettingsSet,
              RenderBindings::kPostProcessSettingsBinding},
             RenderBindings::kPostProcessSettingsBlockName},
        };
        pipeline_desc.topology = RhiPrimitiveTopology::Triangles;
        pipeline_desc.cull_mode = RhiCullMode::None;
        pipeline_desc.depth_test = false;
        pipeline_desc.depth_write = false;
        pipeline_desc.blend_enabled = false;
        pipeline_desc.color_format = RhiFormat::RGBA8Unorm;
        pipeline_desc.depth_format = RhiFormat::Unknown;
        pipeline_desc.debug_name = "PostProcess.Pipeline";
        const auto pipeline = device.createGraphicsPipeline(pipeline_desc);
        if (!pipeline)
        {
            (void)device.destroyShaderModule(vertex.value);
            (void)device.destroyShaderModule(fragment.value);
            HBD_CORE_ERROR("{} pipeline_create_failed reason={}", kPostProcessLogTag, pipeline.error.message);
            return false;
        }

        releasePipeline();
        m_VertexShader = vertex.value;
        m_FragmentShader = fragment.value;
        m_Pipeline = pipeline.value;
        m_ShaderRevision = sources.revision;
        HBD_CORE_INFO("{} pipeline_ready shader_revision={}", kPostProcessLogTag, m_ShaderRevision);
        return true;
    }

    void PostProcessPass::execute(RenderContext& context)
    {
        const std::shared_ptr<Framebuffer>& scene_framebuffer = context.scene_framebuffer;
        if (!scene_framebuffer || !context.shader_library || !context.device || !context.graph_resources)
            return;

        IRenderDevice& device = *context.device;
        const uint32_t width = scene_framebuffer->getWidth();
        const uint32_t height = scene_framebuffer->getHeight();
        const TextureViewHandle output = scene_framebuffer->getColorAttachmentView(
            RenderTargets::kSceneColorAttachment);
        const auto input = context.graph_resources->texture("PostProcessInput");
        if (!input)
        {
            HBD_CORE_ERROR("{} input_resource_unavailable reason={}", kPostProcessLogTag, input.error.message);
            return;
        }
        if (!output || !ensureStaticResources(device) ||
            !ensurePipeline(device, *context.shader_library))
            return;

        PostProcessSettingsGPU settings{};
        settings.enable_tone_mapping = m_Settings.enable_tone_mapping ? 1 : 0;
        settings.enable_gamma_correction = m_Settings.enable_gamma_correction ? 1 : 0;
        settings.exposure = std::max(0.0f, m_Settings.exposure);
        settings.gamma = std::max(0.0001f, m_Settings.gamma);
        const RhiStatus update = device.updateBuffer(m_SettingsBuffer, 0, &settings, sizeof(settings));
        if (!update)
        {
            HBD_CORE_ERROR("{} settings_upload_failed reason={}", kPostProcessLogTag, update.error.message);
            return;
        }

        auto commands = device.createCommandList();
        const auto check = [](const RhiStatus& status, const char* stage)
        {
            if (status)
                return true;
            HBD_CORE_ERROR("{} command_failed stage={} reason={}",
                           kPostProcessLogTag, stage, status.error.message);
            return false;
        };

        if (!commands || !check(commands->begin(), "begin") ||
            !check(commands->copyTexture(output, input.value), "copy_input"))
            return;

        RenderPassDesc pass{};
        ColorAttachmentDesc color{};
        color.texture = output;
        color.clear = false;
        color.store = true;
        pass.colors.push_back(color);
        pass.debug_name = "PostProcess";

        if (!check(commands->beginRenderPass(pass), "begin_render_pass") ||
            !check(commands->setViewport({0.0f, 0.0f,
                                          static_cast<float>(width), static_cast<float>(height)}), "viewport") ||
            !check(commands->bindPipeline(m_Pipeline), "pipeline") ||
            !check(commands->bindTexture(input.value, m_Sampler,
                                         {RenderBindings::kPostProcessTextureSet,
                                          RenderBindings::kPostProcessSceneColorSlot}), "scene_color") ||
            !check(commands->bindUniformBuffer(m_SettingsBuffer,
                                               {RenderBindings::kPostProcessSettingsSet,
                                                RenderBindings::kPostProcessSettingsBinding}), "settings") ||
            !check(commands->bindVertexBuffer(m_VertexBuffer), "vertices") ||
            !check(commands->bindIndexBuffer(m_IndexBuffer), "indices") ||
            !check(commands->drawIndexed(static_cast<uint32_t>(kFullscreenIndices.size())), "draw") ||
            !check(commands->endRenderPass(), "end_render_pass") ||
            !check(commands->end(), "end") ||
            !check(device.submit(*commands), "submit"))
            return;
    }
} // namespace Hybrid
