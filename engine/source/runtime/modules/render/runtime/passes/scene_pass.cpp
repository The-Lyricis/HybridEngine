#include "scene_pass.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <glm/vec4.hpp>

#include "runtime/core/base/macro.h"
#include "runtime/modules/asset/mesh.h"
#include "runtime/modules/render/rhi/render_device.h"
#include "runtime/modules/render/runtime/material_system.h"
#include "runtime/modules/render/runtime/mesh_gpu.h"
#include "runtime/modules/render/runtime/rhi_scene_blocks.h"
#include "runtime/modules/render/runtime/pipeline/render_graph_resources.h"
#include "runtime/modules/render/runtime/render_bindings.h"
#include "runtime/modules/render/runtime/render_shaders.h"
#include "runtime/modules/render/runtime/shader_library.h"

namespace Hybrid
{
    namespace
    {
        constexpr const char* kLogTag = "[ScenePass]";

        std::vector<uint8_t> shaderCode(const std::string& source)
        {
            return {source.begin(), source.end()};
        }

    } // namespace

    ScenePass::~ScenePass() { shutdown(); }

    void ScenePass::releasePipelines()
    {
        if (!m_Device)
            return;
        if (m_OpaquePipeline) (void)m_Device->destroyPipeline(m_OpaquePipeline);
        if (m_TransparentPipeline) (void)m_Device->destroyPipeline(m_TransparentPipeline);
        if (m_VertexShader) (void)m_Device->destroyShaderModule(m_VertexShader);
        if (m_FragmentShader) (void)m_Device->destroyShaderModule(m_FragmentShader);
        m_OpaquePipeline = {};
        m_TransparentPipeline = {};
        m_VertexShader = {};
        m_FragmentShader = {};
        m_ShaderRevision = 0;
    }

    void ScenePass::shutdown()
    {
        if (!m_Device)
            return;
        releasePipelines();
        if (m_ShadowSampler) (void)m_Device->destroySampler(m_ShadowSampler);
        m_ShadowSampler = {};
        for (DrawResources& resources : m_DrawResources)
        {
            if (resources.material_buffer) (void)m_Device->destroyBuffer(resources.material_buffer);
            if (resources.draw_buffer) (void)m_Device->destroyBuffer(resources.draw_buffer);
        }
        m_DrawResources.clear();
        m_Device = nullptr;
    }

    bool ScenePass::ensurePipelines(IRenderDevice& device, ShaderLibrary& shaders)
    {
        if (m_Device && m_Device != &device)
            shutdown();
        m_Device = &device;

        ShaderLibrary::ShaderSourceBundle sources{};
        if (!shaders.getSources(std::string(RenderShaders::kScene.name), sources))
        {
            HBD_CORE_ERROR("{} shader_sources_unavailable", kLogTag);
            return false;
        }
        if (m_OpaquePipeline && m_TransparentPipeline && m_ShaderRevision == sources.revision)
            return true;

        ShaderModuleDesc vertex_desc{};
        vertex_desc.stage = RhiShaderStage::Vertex;
        vertex_desc.code = shaderCode(sources.vertex);
        vertex_desc.debug_name = "Scene.Vertex";
        const auto vertex = device.createShaderModule(vertex_desc);
        if (!vertex)
        {
            HBD_CORE_ERROR("{} vertex_shader_create_failed reason={}", kLogTag, vertex.error.message);
            return false;
        }

        ShaderModuleDesc fragment_desc{};
        fragment_desc.stage = RhiShaderStage::Fragment;
        fragment_desc.code = shaderCode(sources.fragment);
        fragment_desc.debug_name = "Scene.Fragment";
        const auto fragment = device.createShaderModule(fragment_desc);
        if (!fragment)
        {
            (void)device.destroyShaderModule(vertex.value);
            HBD_CORE_ERROR("{} fragment_shader_create_failed reason={}", kLogTag, fragment.error.message);
            return false;
        }

        GraphicsPipelineDesc pipeline_desc{};
        pipeline_desc.vertex_shader = vertex.value;
        pipeline_desc.fragment_shader = fragment.value;
        pipeline_desc.vertex_stride = sizeof(MeshVertex);
        pipeline_desc.vertex_attributes = {
            {0, static_cast<uint32_t>(offsetof(MeshVertex, position)), 3},
            {1, static_cast<uint32_t>(offsetof(MeshVertex, normal)), 3},
            {2, static_cast<uint32_t>(offsetof(MeshVertex, uv)), 2},
            {3, static_cast<uint32_t>(offsetof(MeshVertex, tangent)), 4},
        };
        pipeline_desc.uniform_buffer_bindings = {
            {{RenderBindings::kFrameSet, RenderBindings::kFrameBinding}, "FrameBlock"},
            {{RenderBindings::kSceneLightSet, RenderBindings::kSceneLightBinding},
             RenderBindings::kSceneLightBlockName},
            {{RenderBindings::kSceneLightSet, RenderBindings::kSceneShadowBinding},
             RenderBindings::kSceneShadowBlockName},
            {{RenderBindings::kSceneMaterialSet, RenderBindings::kSceneMaterialBinding},
             RenderBindings::kSceneMaterialBlockName},
            {{RenderBindings::kSceneDrawSet, RenderBindings::kSceneDrawBinding},
             RenderBindings::kSceneDrawBlockName},
        };
        pipeline_desc.texture_bindings = {
            {{RenderBindings::kSceneMaterialSet, RenderBindings::kSceneBaseColorBinding}, RenderBindings::kSceneBaseColorTextureUniform},
            {{RenderBindings::kSceneMaterialSet, RenderBindings::kSceneNormalBinding}, RenderBindings::kSceneNormalUniform},
            {{RenderBindings::kSceneMaterialSet, RenderBindings::kSceneMRBinding}, RenderBindings::kSceneMetallicRoughnessTextureUniform},
            {{RenderBindings::kSceneMaterialSet, RenderBindings::kSceneAOBinding}, RenderBindings::kSceneOcclusionTextureUniform},
            {{RenderBindings::kSceneMaterialSet, RenderBindings::kSceneEmissiveBinding}, RenderBindings::kSceneEmissiveTextureUniform},
            {{RenderBindings::kSceneLightSet, RenderBindings::kSceneShadowTextureFirstBinding}, "u_ShadowMap0"},
            {{RenderBindings::kSceneLightSet, RenderBindings::kSceneShadowTextureFirstBinding + 1}, "u_ShadowMap1"},
            {{RenderBindings::kSceneLightSet, RenderBindings::kSceneShadowTextureFirstBinding + 2}, "u_ShadowMap2"},
            {{RenderBindings::kSceneLightSet, RenderBindings::kSceneShadowTextureFirstBinding + 3}, "u_ShadowMap3"},
        };
        pipeline_desc.topology = RhiPrimitiveTopology::Triangles;
        pipeline_desc.cull_mode = RhiCullMode::Back;
        pipeline_desc.depth_compare = RhiCompareFunction::Less;
        pipeline_desc.depth_test = true;
        pipeline_desc.depth_write = true;
        pipeline_desc.blend_enabled = false;
        pipeline_desc.color_format = RhiFormat::RGBA8Unorm;
        pipeline_desc.depth_format = RhiFormat::Depth32Float;
        pipeline_desc.debug_name = "Scene.OpaquePipeline";
        const auto opaque = device.createGraphicsPipeline(pipeline_desc);
        if (!opaque)
        {
            (void)device.destroyShaderModule(vertex.value);
            (void)device.destroyShaderModule(fragment.value);
            HBD_CORE_ERROR("{} opaque_pipeline_create_failed reason={}", kLogTag, opaque.error.message);
            return false;
        }

        pipeline_desc.depth_write = false;
        pipeline_desc.blend_enabled = true;
        pipeline_desc.debug_name = "Scene.TransparentPipeline";
        const auto transparent = device.createGraphicsPipeline(pipeline_desc);
        if (!transparent)
        {
            (void)device.destroyPipeline(opaque.value);
            (void)device.destroyShaderModule(vertex.value);
            (void)device.destroyShaderModule(fragment.value);
            HBD_CORE_ERROR("{} transparent_pipeline_create_failed reason={}", kLogTag, transparent.error.message);
            return false;
        }

        releasePipelines();
        m_VertexShader = vertex.value;
        m_FragmentShader = fragment.value;
        m_OpaquePipeline = opaque.value;
        m_TransparentPipeline = transparent.value;
        m_ShaderRevision = sources.revision;
        HBD_CORE_INFO("{} pipelines_ready shader_revision={}", kLogTag, m_ShaderRevision);
        return true;
    }

    bool ScenePass::ensureDrawResources(IRenderDevice& device, size_t count)
    {
        while (m_DrawResources.size() < count)
        {
            DrawResources resources{};
            BufferDesc draw_desc{};
            draw_desc.size = sizeof(SceneDrawGPU);
            draw_desc.usage = RhiBufferUsage::Uniform;
            draw_desc.memory = RhiMemoryUsage::CPUToGPU;
            draw_desc.debug_name = "Scene.DrawBlock";
            const auto draw = device.createBuffer(draw_desc);
            if (!draw)
            {
                HBD_CORE_ERROR("{} draw_buffer_create_failed reason={}", kLogTag, draw.error.message);
                return false;
            }
            resources.draw_buffer = draw.value;

            BufferDesc material_desc{};
            material_desc.size = sizeof(SceneMaterialGPU);
            material_desc.usage = RhiBufferUsage::Uniform;
            material_desc.memory = RhiMemoryUsage::CPUToGPU;
            material_desc.debug_name = "Scene.MaterialBlock";
            const auto material = device.createBuffer(material_desc);
            if (!material)
            {
                (void)device.destroyBuffer(resources.draw_buffer);
                HBD_CORE_ERROR("{} material_buffer_create_failed reason={}", kLogTag, material.error.message);
                return false;
            }
            resources.material_buffer = material.value;
            m_DrawResources.push_back(resources);
        }
        return true;
    }

    void ScenePass::execute(RenderContext& context)
    {
        if (!context.packet || !context.device || !context.graph_resources ||
            !context.shader_library || !context.frame_uniform_buffer ||
            !context.light_uniform_buffer || !context.shadow_uniform_buffer)
            return;

        IRenderDevice& device = *context.device;
        const auto scene_color = context.graph_resources->texture("SceneColor");
        const auto entity_id = context.graph_resources->texture("SceneEntityID");
        const auto scene_depth = context.graph_resources->texture("SceneDepth");
        if (!scene_color || !entity_id || !scene_depth)
        {
            HBD_CORE_ERROR("{} graph_resources_unavailable", kLogTag);
            return;
        }
        const auto output_desc = device.textureDesc(scene_color.value);
        if (!output_desc || !ensurePipelines(device, *context.shader_library))
            return;

        const RenderPacket& packet = *context.packet;
        if (!m_ShadowSampler)
        {
            SamplerDesc sampler_desc{};
            sampler_desc.linear_filter = false;
            sampler_desc.clamp_to_edge = true;
            sampler_desc.debug_name = "Scene.ShadowSampler";
            const auto sampler = device.createSampler(sampler_desc);
            if (!sampler)
            {
                HBD_CORE_ERROR("{} shadow_sampler_create_failed reason={}", kLogTag, sampler.error.message);
                return;
            }
            m_ShadowSampler = sampler.value;
        }
        std::array<TextureViewHandle, 4> shadow_views{};
        if (packet.shadow.enabled)
        {
            constexpr std::array<const char*, 4> names{
                "ShadowDepth", "ShadowDepth1", "ShadowDepth2", "ShadowDepth3"
            };
            const auto first = context.graph_resources->texture(names[0]);
            if (!first)
            {
                HBD_CORE_ERROR("{} shadow_depth_unavailable reason={}", kLogTag, first.error.message);
                return;
            }
            for (size_t i = 0; i < shadow_views.size(); ++i)
            {
                const auto view = context.graph_resources->texture(names[i]);
                shadow_views[i] = view ? view.value : first.value;
            }
        }
        const size_t draw_count = packet.opaque_items.size() + packet.transparent_items.size();
        if (!ensureDrawResources(device, draw_count))
            return;

        size_t resource_index = 0;
        const auto upload = [&](const std::vector<RenderDrawItem>& items)
        {
            for (const RenderDrawItem& item : items)
            {
                DrawResources& resources = m_DrawResources[resource_index++];
                SceneDrawGPU draw{};
                draw.model = item.model;
                draw.tint = item.tint;
                draw.ids.x = item.entityID + 1u;
                const SceneMaterialGPU material = BuildSceneMaterialGPU(item.materialGPU);
                const RhiStatus draw_status = device.updateBuffer(resources.draw_buffer, 0, &draw, sizeof(draw));
                const RhiStatus material_status = device.updateBuffer(resources.material_buffer, 0, &material, sizeof(material));
                if (!draw_status || !material_status)
                    HBD_CORE_ERROR("{} parameter_upload_failed", kLogTag);
            }
        };
        upload(packet.opaque_items);
        upload(packet.transparent_items);

        auto commands = device.createCommandList();
        const auto check = [](const RhiStatus& status, const char* stage)
        {
            if (status) return true;
            HBD_CORE_ERROR("{} command_failed stage={} reason={}", kLogTag, stage, status.error.message);
            return false;
        };
        if (!commands || !check(commands->begin(), "begin"))
            return;

        RenderPassDesc pass{};
        ColorAttachmentDesc color{};
        color.texture = scene_color.value;
        color.clear_color[0] = packet.frame.clearColor.r;
        color.clear_color[1] = packet.frame.clearColor.g;
        color.clear_color[2] = packet.frame.clearColor.b;
        color.clear_color[3] = packet.frame.clearColor.a;
        pass.colors.push_back(color);
        ColorAttachmentDesc ids{};
        ids.texture = entity_id.value;
        pass.colors.push_back(ids);
        pass.has_depth = true;
        pass.depth.texture = scene_depth.value;
        pass.depth.clear_depth = 1.0f;
        pass.debug_name = "Scene";
        if (!check(commands->beginRenderPass(pass), "begin_render_pass") ||
            !check(commands->setViewport({0.0f, 0.0f,
                                          static_cast<float>(output_desc.value.width),
                                          static_cast<float>(output_desc.value.height)}), "viewport"))
            return;

        resource_index = 0;
        const auto drawItems = [&](const std::vector<RenderDrawItem>& items, PipelineHandle pipeline)
        {
            if (!check(commands->bindPipeline(pipeline), "pipeline") ||
                !check(commands->bindUniformBuffer(context.frame_uniform_buffer,
                                                   {RenderBindings::kFrameSet,
                                                    RenderBindings::kFrameBinding}), "frame_block") ||
                !check(commands->bindUniformBuffer(context.light_uniform_buffer,
                                                   {RenderBindings::kSceneLightSet,
                                                    RenderBindings::kSceneLightBinding}), "light_block") ||
                !check(commands->bindUniformBuffer(context.shadow_uniform_buffer,
                                                   {RenderBindings::kSceneLightSet,
                                                    RenderBindings::kSceneShadowBinding}), "shadow_block"))
                return false;
            if (packet.shadow.enabled)
                for (uint32_t i = 0; i < shadow_views.size(); ++i)
                    if (!check(commands->bindTexture(shadow_views[i], m_ShadowSampler,
                                                     {RenderBindings::kSceneLightSet,
                                                      RenderBindings::kSceneShadowTextureFirstBinding + i}),
                               "shadow_texture")) return false;
            for (const RenderDrawItem& item : items)
            {
                DrawResources& resources = m_DrawResources[resource_index++];
                if (!item.materialGPU || !item.meshGPU || !item.meshGPU->rhi_vertex_buffer ||
                    !item.meshGPU->rhi_index_buffer || item.indexCount == 0)
                    continue;
                if (!item.materialGPU->bindTextures(*commands) ||
                    !check(commands->bindUniformBuffer(resources.material_buffer,
                                                       {RenderBindings::kSceneMaterialSet,
                                                        RenderBindings::kSceneMaterialBinding}), "material_block") ||
                    !check(commands->bindUniformBuffer(resources.draw_buffer,
                                                       {RenderBindings::kSceneDrawSet,
                                                        RenderBindings::kSceneDrawBinding}), "draw_block") ||
                    !check(commands->bindVertexBuffer(item.meshGPU->rhi_vertex_buffer), "vertex_buffer") ||
                    !check(commands->bindIndexBuffer(item.meshGPU->rhi_index_buffer), "index_buffer") ||
                    !check(commands->drawIndexed(item.indexCount, item.indexOffset), "draw_indexed"))
                    return false;
            }
            return true;
        };

        if (!drawItems(packet.opaque_items, m_OpaquePipeline) ||
            !drawItems(packet.transparent_items, m_TransparentPipeline) ||
            !check(commands->endRenderPass(), "end_render_pass") ||
            !check(commands->end(), "end") ||
            !check(device.submit(*commands), "submit"))
            return;
    }
} // namespace Hybrid
