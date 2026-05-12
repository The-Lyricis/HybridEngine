#include "render_system.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <glad/gl.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <array>
#include <filesystem>
#include <limits>
#include <string>
#include <unordered_set>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include "runtime/modules/render/public/renderer.h"
#include "runtime/modules/render/public/render_command.h"
#include "runtime/modules/render/public/buffer.h"
#include "runtime/modules/render/public/vertex_array.h"
#include "runtime/modules/render/public/shader.h"
#include "runtime/modules/render/public/framebuffer.h"
#include "runtime/modules/render/public/texture.h"
#include "runtime/modules/render/runtime/render_binding_layout.h"
#include "runtime/modules/render/runtime/render_bindings.h"
#include "runtime/modules/render/runtime/render_shaders.h"
#include "runtime/modules/render/runtime/render_targets.h"
#include "runtime/modules/scene/scene.h"
#include "runtime/modules/scene/components.h"
#include "runtime/core/base/intersection.h"
#include "runtime/core/base/macro.h"
#include "runtime/core/base/math_util.h"
#include "runtime/modules/asset/asset_manager.h"
#include "runtime/modules/asset/cubemap_image.h"
#include "runtime/modules/asset/material.h"
#include "runtime/modules/asset/mesh.h"
#include <runtime/modules/scene/components/collider_component.h>


namespace Hybrid
{
    namespace RU = RenderUniforms;

    namespace
    {
        constexpr const char* kRenderSystemLogTag = "[RenderSystem]";

        FramebufferSpec makeMainFramebufferSpec(uint32_t width, uint32_t height)
        {
            FramebufferSpec spec{};
            spec.width = width;
            spec.height = height;
            spec.attachment_spec = {
                FramebufferTextureFormat::RGBA8,
                FramebufferTextureFormat::R32UI,
                FramebufferTextureFormat::Depth32F
            };
            return spec;
        }

        FramebufferSpec makeSelectionFramebufferSpec(uint32_t width, uint32_t height)
        {
            FramebufferSpec spec{};
            spec.width = width;
            spec.height = height;
            spec.attachment_spec = {
                FramebufferTextureFormat::R8,
                FramebufferTextureFormat::Depth32F
            };
            return spec;
        }

        FramebufferSpec makeShadowFramebufferSpec(uint32_t size)
        {
            FramebufferSpec spec{};
            spec.width = size;
            spec.height = size;
            spec.attachment_spec = {
                FramebufferTextureFormat::Depth32F
            };
            return spec;
        }

        std::array<glm::vec3, 8> buildCameraFrustumSliceCorners(const glm::mat4& view,
                                                                const glm::mat4& proj,
                                                                float near_distance,
                                                                float far_distance)
        {
            std::array<glm::vec3, 8> corners{};
            const glm::mat4 inv_view = glm::inverse(view);
            const float tan_half_fov = 1.0f / std::max(proj[1][1], 1e-6f);
            const float aspect = proj[1][1] / std::max(proj[0][0], 1e-6f);

            const auto write_plane = [&](float distance, size_t base_index)
            {
                const float half_height = distance * tan_half_fov;
                const float half_width = half_height * aspect;

                const std::array<glm::vec3, 4> local = {
                    glm::vec3(-half_width, -half_height, -distance),
                    glm::vec3( half_width, -half_height, -distance),
                    glm::vec3( half_width,  half_height, -distance),
                    glm::vec3(-half_width,  half_height, -distance),
                };

                for (size_t i = 0; i < local.size(); ++i)
                    corners[base_index + i] = glm::vec3(inv_view * glm::vec4(local[i], 1.0f));
            };

            write_plane(near_distance, 0);
            write_plane(far_distance, 4);
            return corners;
        }

        std::array<glm::vec3, 8> buildWorldSpaceFrustumCorners(const glm::mat4& inv_view_proj)
        {
            std::array<glm::vec3, 8> corners{};
            size_t index = 0;
            for (int z = 0; z < 2; ++z)
            {
                const float ndc_z = (z == 0) ? -1.0f : 1.0f;
                for (int y = 0; y < 2; ++y)
                {
                    const float ndc_y = (y == 0) ? -1.0f : 1.0f;
                    for (int x = 0; x < 2; ++x)
                    {
                        const float ndc_x = (x == 0) ? -1.0f : 1.0f;
                        glm::vec4 corner = inv_view_proj * glm::vec4(ndc_x, ndc_y, ndc_z, 1.0f);
                        if (std::abs(corner.w) > 1e-6f)
                            corner /= corner.w;
                        corners[index++] = glm::vec3(corner);
                    }
                }
            }
            return corners;
        }

        uint32_t encodeEntityID(uint32_t entity_id)
        {
            return entity_id + 1u;
        }

        uint32_t decodeEntityID(uint32_t encoded_id)
        {
            return (encoded_id == 0) ? kInvalidEntityID : (encoded_id - 1u);
        }

        // Returns false if no valid camera found.
    }

    void RenderSystem::setAssetManager(std::shared_ptr<AssetManager> mgr)
    {
        m_AssetManager = std::move(mgr);
        m_MaterialSystem.setAssetManager(m_AssetManager);
        m_TextureUploader = TextureUploader::Create();
        m_CubemapCache.clear();
        m_DefaultCubemapTexture.reset();
    }

    void RenderSystem::initialize(void *glfwWindowHandle)
    {
        if (m_Initialized)
            return;

        Renderer::initialize();
        if (!loadBuiltinShaders())
        {
            HBD_CORE_ERROR("{} initialize_failed reason=load_builtin_shaders_failed",
                           kRenderSystemLogTag);
            return;
        }

        GLFWwindow *window = static_cast<GLFWwindow *>(glfwWindowHandle);
        int w = 0, h = 0;
        glfwGetFramebufferSize(window, &w, &h);

        const uint32_t width = static_cast<uint32_t>(std::max(1, w));
        const uint32_t height = static_cast<uint32_t>(std::max(1, h));
        ensureSceneViewRenderTargets(width, height);
        ensureFramebuffer(m_GameFB, makeMainFramebufferSpec(width, height));
        const uint32_t shadow_cascade_count =
            std::clamp(m_DirectionalShadowSettings.cascade_count, 1u, kMaxDirectionalShadowCascades);
        for (uint32_t cascade_index = 0; cascade_index < shadow_cascade_count; ++cascade_index)
            ensureFramebuffer(m_ShadowCascadeFBs[cascade_index], makeShadowFramebufferSpec(m_DirectionalShadowSettings.map_resolution));
        for (uint32_t cascade_index = shadow_cascade_count; cascade_index < kMaxDirectionalShadowCascades; ++cascade_index)
            m_ShadowCascadeFBs[cascade_index].reset();
        ensureGlobalUniformBuffers();
        if (!m_FrameUBO || !m_LightUBO)
        {
            HBD_CORE_ERROR("{} initialize_failed reason=ubo_creation_failed",
                           kRenderSystemLogTag);
            return;
        }
        configureShaderBindings();

        m_Initialized = true;

        HBD_CORE_INFO("{} initialize_completed framebuffer_size={}x{}",
                      kRenderSystemLogTag,
                      width,
                      height);
    }

    void RenderSystem::update(float dt)
    {
        if (!m_Initialized || dt <= 0.0f)
            return;

        m_ShaderReloadTimer += dt;
        if (m_ShaderReloadTimer < m_ShaderReloadInterval)
            return;

        m_ShaderReloadTimer = 0.0f;
        m_ShaderLibrary.reloadChanged();

        m_SceneShader = m_ShaderLibrary.get("Scene");
        m_SkyboxShader = m_ShaderLibrary.get("Skybox");
        m_ShadowShader = m_ShaderLibrary.get("ShadowDepth");
        m_ColliderDebugShader = m_ShaderLibrary.get("ColliderDebug");
        configureShaderBindings();
    }

    MeshGPU *RenderSystem::getOrCreateMeshGPU(AssetID id, const std::shared_ptr<Mesh> &mesh)
    {
        if (!mesh)
            return nullptr;
        if (auto it = m_MeshCache.find(id); it != m_MeshCache.end())
            return &it->second;

        MeshGPU mgpu;
        const auto &verts = mesh->getVertices();
        const auto &inds = mesh->getIndices();

        mgpu.vb = VertexBuffer::Create(verts.data(), static_cast<uint32_t>(verts.size() * sizeof(MeshVertex)));
        mgpu.ib = IndexBuffer::Create(inds.data(), static_cast<uint32_t>(inds.size()));
        mgpu.vao = VertexArray::Create();

        VertexLayout layout;
        layout.stride = sizeof(MeshVertex);
        layout.attributes = {
            {0, 3, static_cast<uint32_t>(offsetof(MeshVertex, position)), false},
            {1, 3, static_cast<uint32_t>(offsetof(MeshVertex, normal)), false},
            {2, 2, static_cast<uint32_t>(offsetof(MeshVertex, uv)), false},
            {3, 4, static_cast<uint32_t>(offsetof(MeshVertex, tangent)), false},
        };
        mgpu.vao->setVertexBuffer(mgpu.vb, layout);
        mgpu.vao->setIndexBuffer(mgpu.ib);
        mgpu.submeshes = mesh->getSubmeshes();

        auto [it, inserted] = m_MeshCache.emplace(id, std::move(mgpu));
        return &it->second;
    }

    bool RenderSystem::loadBuiltinShaders()
    {
        m_ShaderLibrary.setRoot(std::filesystem::path(HYBRID_PROJECT_ROOT_DIR) / "engine/shader");

        bool scene_ok = false;
        bool skybox_ok = false;
        bool shadow_ok = false;
        bool collider_debug_ok = false;
        bool selection_mask_ok = false;
        bool selection_overlay_ok = false;

        for (const auto& shader_desc : RenderShaders::kBuiltinShaders)
        {
            const bool loaded = m_ShaderLibrary.load(std::string(shader_desc.name),
                                                     std::string(shader_desc.vertex),
                                                     std::string(shader_desc.fragment));
            if (shader_desc.name == RenderShaders::kScene.name)
                scene_ok = loaded;
            else if (shader_desc.name == RenderShaders::kSkybox.name)
                skybox_ok = loaded;
            else if (shader_desc.name == RenderShaders::kShadowDepth.name)
                shadow_ok = loaded;
            else if (shader_desc.name == RenderShaders::kColliderDebug.name)
                collider_debug_ok = loaded;
            else if (shader_desc.name == RenderShaders::kSelectionMask.name)
                selection_mask_ok = loaded;
            else if (shader_desc.name == RenderShaders::kSelectionOverlay.name)
                selection_overlay_ok = loaded;
        }

        m_SceneShader = m_ShaderLibrary.get(std::string(RenderShaders::kScene.name));
        m_SkyboxShader = m_ShaderLibrary.get(std::string(RenderShaders::kSkybox.name));
        m_ShadowShader = m_ShaderLibrary.get(std::string(RenderShaders::kShadowDepth.name));
        m_ColliderDebugShader = m_ShaderLibrary.get(std::string(RenderShaders::kColliderDebug.name));
        configureShaderBindings();
        HBD_CORE_INFO("{} builtin_shaders_loaded scene={} skybox={} shadow={} collider_debug={} selection_mask={} selection_overlay={} scene_shader_ready={} skybox_shader_ready={} shadow_shader_ready={} collider_debug_shader_ready={}",
                      kRenderSystemLogTag,
                      scene_ok ? "true" : "false",
                      skybox_ok ? "true" : "false",
                      shadow_ok ? "true" : "false",
                      collider_debug_ok ? "true" : "false",
                      selection_mask_ok ? "true" : "false",
                      selection_overlay_ok ? "true" : "false",
                      m_SceneShader ? "true" : "false",
                      m_SkyboxShader ? "true" : "false",
                      m_ShadowShader ? "true" : "false",
                      m_ColliderDebugShader ? "true" : "false");
        return scene_ok && skybox_ok && shadow_ok && collider_debug_ok && selection_mask_ok && selection_overlay_ok &&
               m_SceneShader && m_SkyboxShader && m_ShadowShader && m_ColliderDebugShader;
    }

    void RenderSystem::ensureGlobalUniformBuffers()
    {
        if (!m_FrameUBO)
        {
            m_FrameUBO = UniformBuffer::Create(sizeof(RU::FrameUBOData));
            if (!m_FrameUBO)
            {
                HBD_CORE_ERROR("{} ubo_create_failed block={} size={}",
                               kRenderSystemLogTag,
                               RU::kFrameBlockName,
                               sizeof(RU::FrameUBOData));
            }
        }
        if (!m_LightUBO)
        {
            m_LightUBO = UniformBuffer::Create(sizeof(RU::LightUBOData));
            if (!m_LightUBO)
            {
                HBD_CORE_ERROR("{} ubo_create_failed block={} size={}",
                               kRenderSystemLogTag,
                               RU::kLightBlockName,
                               sizeof(RU::LightUBOData));
            }
        }
    }

    void RenderSystem::configureShaderBindings()
    {
        if (m_SceneShader)
        {
            m_SceneShader->bind();
            ApplyStaticUniformBlockBindings(*m_SceneShader, GetSceneBindingLayout());
            ApplyStaticTextureBindings(*m_SceneShader, GetSceneBindingLayout());
        }

        if (auto selection_mask_shader = m_ShaderLibrary.get(std::string(RenderShaders::kSelectionMask.name)))
        {
            selection_mask_shader->bind();
            ApplyStaticUniformBlockBindings(*selection_mask_shader, GetSelectionMaskBindingLayout());
            ApplyStaticTextureBindings(*selection_mask_shader, GetSelectionMaskBindingLayout());
        }

        if (m_SkyboxShader)
        {
            m_SkyboxShader->bind();
            ApplyStaticUniformBlockBindings(*m_SkyboxShader, GetSkyboxBindingLayout());
            ApplyStaticTextureBindings(*m_SkyboxShader, GetSkyboxBindingLayout());
        }

        if (m_ShadowShader)
        {
            m_ShadowShader->bind();
            ApplyStaticTextureBindings(*m_ShadowShader, GetShadowDepthBindingLayout());
        }

        if (auto selection_overlay_shader = m_ShaderLibrary.get(std::string(RenderShaders::kSelectionOverlay.name)))
        {
            selection_overlay_shader->bind();
            ApplyStaticTextureBindings(*selection_overlay_shader, GetSelectionOverlayBindingLayout());
        }

        if (auto post_process_shader = m_ShaderLibrary.get(std::string(RenderShaders::kPostProcess.name)))
        {
            post_process_shader->bind();
            ApplyStaticTextureBindings(*post_process_shader, GetPostProcessBindingLayout());
        }
    }

    void RenderSystem::updateFrameUBO(const RenderPacket& packet, const glm::vec2& viewport_size)
    {
        if (!m_FrameUBO)
            return;

        RU::FrameUBOData data{};
        data.view = packet.frame.view;
        data.proj = packet.frame.proj;
        data.viewProj = packet.frame.viewProj;
        data.cameraPos = glm::vec4(packet.frame.cameraPos, 1.0f);

        const float width = viewport_size.x;
        const float height = viewport_size.y;
        data.viewport = glm::vec4(width,
                                  height,
                                  width > 0.0f ? 1.0f / width : 0.0f,
                                  height > 0.0f ? 1.0f / height : 0.0f);

        m_FrameUBO->setData(&data, sizeof(RU::FrameUBOData));
        m_FrameUBO->bindBase(RU::kFrameUBOBinding);
    }

    void RenderSystem::updateLightUBO(const RenderPacket& packet)
    {
        if (!m_LightUBO)
            return;

        RU::LightUBOData data{};
        data.dirLight.colorIntensity = glm::vec4(packet.lights.dir.color, packet.lights.dir.intensity);
        data.dirLight.direction = glm::vec4(packet.lights.dir.direction, 0.0f);

        const int point_count = std::min<int>(static_cast<int>(packet.lights.points.size()), RU::kMaxPointLights);
        for (int i = 0; i < point_count; ++i)
        {
            const auto& point = packet.lights.points[static_cast<size_t>(i)];
            data.pointLights[static_cast<size_t>(i)].colorIntensity = glm::vec4(point.color, point.intensity);
            data.pointLights[static_cast<size_t>(i)].positionRange = glm::vec4(point.position, point.range);
        }

        data.counts = glm::ivec4(point_count, 0, 0, 0);

        m_LightUBO->setData(&data, sizeof(RU::LightUBOData));
        m_LightUBO->bindBase(RU::kLightUBOBinding);
    }

    void RenderSystem::ensureFramebuffer(std::shared_ptr<Framebuffer>& framebuffer, const FramebufferSpec& spec)
    {
        if (!framebuffer)
        {
            framebuffer = Framebuffer::Create(spec);
        }
        else
        {
            framebuffer->resize(spec.width, spec.height);
        }
    }

    void RenderSystem::ensureSceneViewRenderTargets(uint32_t w, uint32_t h)
    {
        const uint32_t width = std::max(1u, w);
        const uint32_t height = std::max(1u, h);
        ensureFramebuffer(m_SceneFB, makeMainFramebufferSpec(width, height));
        ensureFramebuffer(m_SelectionFB, makeSelectionFramebufferSpec(width, height));
    }

    uint32_t RenderSystem::getSceneColorTexture() const
    {
        return m_SceneFB ? m_SceneFB->getColorAttachmentRendererID(RenderTargets::kSceneColorAttachment) : 0;
    }

    uint32_t RenderSystem::getGameColorTexture() const
    {
        return m_GameFB ? m_GameFB->getColorAttachmentRendererID(RenderTargets::kSceneColorAttachment) : 0;
    }

    void RenderSystem::onWindowResize(uint32_t width, uint32_t height)
    {
        if (!m_Initialized)
            return;
        ensureSceneViewRenderTargets(width, height);
        ensureFramebuffer(m_GameFB, makeMainFramebufferSpec(width, height));
    }

    void RenderSystem::invalidateAsset(AssetID id, AssetType type)
    {
        if (id.value == 0)
            return;

        switch (type)
        {
        case AssetType::Mesh:
            m_MeshCache.erase(id);
            break;
        case AssetType::Material:
            m_MaterialSystem.invalidateMaterial(id);
            break;
        case AssetType::Texture2D:
        case AssetType::TextureCube:
            m_MaterialSystem.invalidateTexture(id);
            m_CubemapCache.erase(id);
            m_DefaultCubemapTexture.reset();
            break;
        default:
            break;
        }
    }

    TexturePtr RenderSystem::getOrCreateCubemapTexture(AssetID id)
    {
        if (!m_AssetManager)
            return nullptr;

        if (!m_TextureUploader)
            m_TextureUploader = TextureUploader::Create();
        if (!m_TextureUploader)
            return nullptr;

        if (id.value == 0)
            return getDefaultCubemapTexture();

        if (auto it = m_CubemapCache.find(id); it != m_CubemapCache.end())
            return it->second;

        auto image = m_AssetManager->loadSync<CubemapImageData>(id);
        if (!image || !image->isValid())
            return getDefaultCubemapTexture();

        TexturePtr cubemap = m_TextureUploader->uploadTextureCube(*image);
        if (!cubemap)
            return getDefaultCubemapTexture();

        m_CubemapCache[id] = cubemap;
        return cubemap;
    }

    TexturePtr RenderSystem::getDefaultCubemapTexture()
    {
        if (m_DefaultCubemapTexture)
            return m_DefaultCubemapTexture;
        if (!m_AssetManager)
            return nullptr;

        if (!m_TextureUploader)
            m_TextureUploader = TextureUploader::Create();
        if (!m_TextureUploader)
            return nullptr;

        auto image = m_AssetManager->getDefault<CubemapImageData>();
        if (!image || !image->isValid())
            return nullptr;

        m_DefaultCubemapTexture = m_TextureUploader->uploadTextureCube(*image);
        return m_DefaultCubemapTexture;
    }

    
    RenderPacket RenderSystem::buildRenderPacket(const FrameContext& frame_context,
                                                 RenderFlags flags,
                                                 const EditorRenderExt* editor_ext,
                                                 bool cache_editor_camera_state)
    {
        std::shared_ptr<Scene> scene = frame_context.scene ? frame_context.scene : m_Scene;
        FrameViewResolveInput view_input{};
        view_input.scene = scene;
        view_input.frame = &frame_context;
        view_input.editor_ext = editor_ext;
        view_input.flags = flags;
        view_input.resolve_cubemap = [this](AssetID id)
        {
            return getOrCreateCubemapTexture(id);
        };
        const FrameViewResolveResult view_result = m_FrameViewResolver.resolve(view_input);

        if (cache_editor_camera_state)
        {
            m_LastView = view_result.view.frame.view;
            m_LastProj = view_result.view.frame.proj;
        }

        ShadowFrameBuildInput shadow_input{};
        shadow_input.view = &view_result.view;
        shadow_input.settings = &m_DirectionalShadowSettings;
        RenderShadowData shadow_data{};
        m_ShadowFrameBuilder.build(shadow_input, shadow_data);

        RenderPacketBuildInput packet_input{};
        packet_input.scene = scene;
        packet_input.view = view_result.view;
        packet_input.environment = view_result.environment;
        packet_input.shadow = &shadow_data;
        packet_input.editor_ext = editor_ext;
        packet_input.asset_manager = m_AssetManager;
        packet_input.material_system = &m_MaterialSystem;
        packet_input.resolve_mesh_gpu = [this](AssetID id, const std::shared_ptr<Mesh>& mesh)
        {
            return getOrCreateMeshGPU(id, mesh);
        };
        return m_RenderPacketBuilder.build(packet_input);
    }

    void RenderSystem::updateStatsFromPacket(const RenderPacket& packet, float render_cpu_time_ms)
    {
        m_Stats.render_cpu_time_ms = std::max(0.0f, render_cpu_time_ms);
        m_Stats.scene_renderers = packet.scene_renderers;
        m_Stats.scene_submeshes = packet.scene_submeshes;
        m_Stats.submitted_opaque_items = static_cast<uint32_t>(packet.opaque_items.size());
        m_Stats.submitted_transparent_items = static_cast<uint32_t>(packet.transparent_items.size());
        m_Stats.shadow_caster_items = static_cast<uint32_t>(packet.shadow_caster_items.size());
        m_Stats.point_lights = static_cast<uint32_t>(packet.lights.points.size());
        m_Stats.tested_items = packet.tested_items;
        m_Stats.culled_items = packet.culled_items;

        uint32_t draw_calls = 0;
        uint32_t triangles = 0;
        std::unordered_set<uint32_t> submitted_entities;

        auto accumulate_queue = [&](const std::vector<RenderDrawItem>& items)
        {
            for (const RenderDrawItem& item : items)
            {
                if (!item.meshGPU || !item.materialGPU || item.indexCount == 0)
                    continue;
                ++draw_calls;
                triangles += item.indexCount / 3;
                submitted_entities.insert(item.entityID);
            }
        };

        accumulate_queue(packet.opaque_items);
        accumulate_queue(packet.transparent_items);

        if (packet.frame.useSkyboxClear && packet.environment.skyboxTexture)
        {
            ++draw_calls;
            triangles += 12;
        }

        m_Stats.submitted_draw_calls = draw_calls;
        m_Stats.submitted_triangles = triangles;
        m_Stats.submitted_entities = static_cast<uint32_t>(submitted_entities.size());
    }

    void RenderSystem::renderFrame(const FrameContext& frame_context,
                                   RenderFlags flags,
                                   const EditorRenderExt* editor_ext)
    {
        m_Stats.frame_time_ms = std::max(0.0f, frame_context.dt * 1000.0f);
        m_Stats.fps = frame_context.dt > 1e-6f ? (1.0f / frame_context.dt) : 0.0f;

        auto make_pipeline_callbacks =
            [this]()
        {
            RenderPipelineCallbacks callbacks;
            callbacks.scene = [this](RenderContext& context)
            {
                m_ScenePass.execute(context);
            };
            callbacks.skybox = [this](RenderContext& context)
            {
                m_SkyboxPass.execute(context);
            };
            callbacks.picking = [this](RenderContext& context)
            {
                m_PickingPass.execute(context);
            };
            callbacks.selection_mask = [this](RenderContext& context)
            {
                m_SelectionMaskPass.execute(context);
            };
            callbacks.selection_overlay = [this](RenderContext& context)
            {
                m_SelectionOverlayPass.execute(context);
            };
            callbacks.world_gizmo = [this](RenderContext& context)
            {
                m_GizmoPass.execute(context);
            };
            callbacks.overlay_gizmo = [this](RenderContext& context)
            {
                m_OverlayGizmoPass.execute(context);
            };
            callbacks.grid = [this](RenderContext& context)
            {
                m_GridPass.execute(context);
            };
            callbacks.shadow = [this](RenderContext& context)
            {
                m_ShadowPass.execute(context);
            };
            callbacks.post_process = [this](RenderContext& context)
            {
                m_PostProcessPass.execute(context);
            };
            return callbacks;
        };

        const auto execute_render = [this, &make_pipeline_callbacks](const FrameContext& current_frame,
                                                                     const RenderPacket& packet,
                                                                     const EditorSelectionState* editor_selection,
                                                                     const EditorPostProcessState* post_process,
                                                                     RenderFlags current_flags,
                                                                     const ResolvedRenderTargets& targets)
        {
            RenderContextBuildInput context_input{};
            context_input.frame = &current_frame;
            context_input.packet = &packet;
            context_input.editor_selection = editor_selection;
            context_input.flags = current_flags;
            context_input.window_handle = current_frame.window_handle;
            context_input.targets = targets;
            context_input.selection_overlay_style = &m_SelectionOverlayStyle;
            context_input.shader_library = &m_ShaderLibrary;
            context_input.scene_shader = m_SceneShader;
            context_input.skybox_shader = m_SkyboxShader;
            context_input.shadow_shader = m_ShadowShader;
            context_input.collider_debug_shader = m_ColliderDebugShader;

            RenderContext context = m_RenderContextBuilder.build(context_input);
            updateFrameUBO(packet, current_frame.viewport_size);
            updateLightUBO(packet);

            PostProcessPass::Settings post_process_settings{};
            if (post_process)
            {
                post_process_settings.enable_tone_mapping = post_process->enable_tone_mapping;
                post_process_settings.enable_gamma_correction = post_process->enable_gamma_correction;
                post_process_settings.exposure = post_process->exposure;
                post_process_settings.gamma = post_process->gamma;
            }
            m_PostProcessPass.setSettings(post_process_settings);

            const auto render_begin = std::chrono::steady_clock::now();
            m_RenderPipeline.execute(context, make_pipeline_callbacks());
            const auto render_end = std::chrono::steady_clock::now();
            updateStatsFromPacket(packet, std::chrono::duration<float, std::milli>(render_end - render_begin).count());
        };

        void* window_handle = frame_context.window_handle;
        if (!window_handle)
            return;

        if (!m_Initialized)
            initialize(window_handle);

        if (flags == RenderFlags::None)
            return;

        if (editor_ext && (editor_ext->render_scene_view || editor_ext->render_game_view))
        {
            bool rendered_any = false;

            if (editor_ext->render_scene_view &&
                editor_ext->scene_viewport_size.x > 0.0f &&
                editor_ext->scene_viewport_size.y > 0.0f)
            {
                FrameContext scene_frame = frame_context;
                scene_frame.viewport_size = editor_ext->scene_viewport_size;
                ensureSceneViewRenderTargets(static_cast<uint32_t>(scene_frame.viewport_size.x),
                                             static_cast<uint32_t>(scene_frame.viewport_size.y));

                EditorRenderExt scene_ext = *editor_ext;
                scene_ext.use_game_camera = false;
                auto scene_packet = buildRenderPacket(scene_frame, flags, &scene_ext, true);
                ResolvedRenderTargets scene_targets{};
                scene_targets.framebuffer = m_SceneFB;
                scene_targets.scene_framebuffer = m_SceneFB;
                scene_targets.selection_framebuffer = m_SelectionFB;
                scene_targets.shadow_framebuffer = m_ShadowCascadeFBs[0];
                scene_targets.shadow_cascade_framebuffers = &m_ShadowCascadeFBs;
                execute_render(scene_frame, scene_packet, &scene_ext.selection, &scene_ext.post_process, flags, scene_targets);
                rendered_any = true;
            }

            if (editor_ext->render_game_view &&
                editor_ext->game_viewport_size.x > 0.0f &&
                editor_ext->game_viewport_size.y > 0.0f)
            {
                FrameContext game_frame = frame_context;
                game_frame.viewport_size = editor_ext->game_viewport_size;
                ensureFramebuffer(m_GameFB,
                                  makeMainFramebufferSpec(static_cast<uint32_t>(game_frame.viewport_size.x),
                                                          static_cast<uint32_t>(game_frame.viewport_size.y)));

                EditorRenderExt game_ext = *editor_ext;
                game_ext.use_game_camera = true;
                game_ext.has_editor_camera = false;
                const RenderFlags game_flags = RenderFlags::Scene | RenderFlags::Shadow;
                auto game_packet = buildRenderPacket(game_frame, game_flags, &game_ext, false);
                ResolvedRenderTargets game_targets{};
                game_targets.framebuffer = m_GameFB;
                game_targets.scene_framebuffer = m_GameFB;
                game_targets.selection_framebuffer = nullptr;
                game_targets.shadow_framebuffer = m_ShadowCascadeFBs[0];
                game_targets.shadow_cascade_framebuffers = &m_ShadowCascadeFBs;
                execute_render(game_frame, game_packet, &game_ext.selection, nullptr, game_flags, game_targets);
                rendered_any = true;
            }

            if (rendered_any)
                return;
        }

        if (frame_context.viewport_size.x <= 0.0f || frame_context.viewport_size.y <= 0.0f)
            return;

        ensureSceneViewRenderTargets(static_cast<uint32_t>(frame_context.viewport_size.x),
                                     static_cast<uint32_t>(frame_context.viewport_size.y));

        auto packet = buildRenderPacket(frame_context, flags, editor_ext, true);
        ResolvedRenderTargets targets{};
        targets.framebuffer = m_SceneFB;
        targets.scene_framebuffer = m_SceneFB;
        targets.selection_framebuffer = m_SelectionFB;
        targets.shadow_framebuffer = m_ShadowCascadeFBs[0];
        targets.shadow_cascade_framebuffers = &m_ShadowCascadeFBs;
        execute_render(frame_context,
                       packet,
                       editor_ext ? &editor_ext->selection : nullptr,
                       editor_ext ? &editor_ext->post_process : nullptr,
                       flags,
                       targets);
    }
    uint32_t RenderSystem::readEntityID(int x, int y) const
    {
        if (!m_SceneFB)
            return kInvalidEntityID;
        if (x < 0 || y < 0)
            return kInvalidEntityID;
        if (x >= static_cast<int>(m_SceneFB->getWidth()) ||
            y >= static_cast<int>(m_SceneFB->getHeight()))
            return kInvalidEntityID;

        const uint32_t encoded_id = m_SceneFB->readPixelUInt(RenderTargets::kSceneEntityIDAttachment, x, y);
        return decodeEntityID(encoded_id);
    }

}
