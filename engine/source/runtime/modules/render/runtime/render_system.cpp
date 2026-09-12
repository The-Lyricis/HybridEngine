#include "render_system.h"

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

        RenderViewId resolveViewId(const RenderViewRequest& view)
        {
            if (view.id != kInvalidRenderViewId)
                return view.id;
            uint64_t hash = 1469598103934665603ull;
            for (unsigned char c : view.name)
            {
                hash ^= c;
                hash *= 1099511628211ull;
            }
            return hash == kInvalidRenderViewId ? 1 : hash;
        }

        // Returns false if no valid camera found.
    }

    RenderSystem::RenderSystem(std::unique_ptr<IRenderDevice> device,
                               std::unique_ptr<ISwapchain> swapchain)
        : m_RenderDevice(std::move(device)),
          m_Swapchain(std::move(swapchain)),
          m_RenderPipeline(std::make_unique<RenderPipeline>())
    {
    }

    RenderSystem::~RenderSystem()
    {
        shutdown();
    }

    void RenderSystem::setAssetManager(std::shared_ptr<AssetManager> mgr)
    {
        m_AssetManager = std::move(mgr);
        m_MaterialSystem.setAssetManager(m_AssetManager);
        m_TextureUploader = TextureUploader::Create();
        m_CubemapCache.clear();
        m_DefaultCubemapTexture.reset();
    }

    void RenderSystem::initialize(uint32_t framebuffer_width, uint32_t framebuffer_height)
    {
        if (m_Initialized)
            return;
        if (!m_RenderPipeline)
            m_RenderPipeline = std::make_unique<RenderPipeline>();
        if (!m_RenderDevice)
        {
            HBD_CORE_ERROR("{} initialize_failed reason=render_device_missing", kRenderSystemLogTag);
            return;
        }

        Renderer::initialize();
        if (!loadBuiltinShaders())
        {
            HBD_CORE_ERROR("{} initialize_failed reason=load_builtin_shaders_failed",
                           kRenderSystemLogTag);
            return;
        }

        const uint32_t width = std::max(1u, framebuffer_width);
        const uint32_t height = std::max(1u, framebuffer_height);
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

    void RenderSystem::shutdown()
    {
        m_MaterialSystem.shutdown();
        m_ScenePass.shutdown();
        m_GizmoPass = GizmoPass{};
        m_SkyboxPass = SkyboxPass{};
        m_PostProcessPass.shutdown();
        m_SelectionOverlayPass.shutdown();
        clearMeshCache();
        m_CubemapCache.clear();
        m_DefaultCubemapTexture.reset();
        m_TextureUploader.reset();
        m_ViewTargets.clear();
        m_RenderFrameIndex = 0;
        for (auto& framebuffer : m_ShadowCascadeFBs)
            framebuffer.reset();
        m_FrameUBO.reset();
        m_LightUBO.reset();
        if (m_RenderDevice && m_RhiFrameUniformBuffer)
            (void)m_RenderDevice->destroyBuffer(m_RhiFrameUniformBuffer);
        m_RhiFrameUniformBuffer = {};
        m_SceneShader.reset();
        m_SkyboxShader.reset();
        m_ShadowShader.reset();
        m_ColliderDebugShader.reset();
        m_ShaderLibrary.clear();
        m_AssetManager.reset();
        m_Scene.reset();
        Renderer::shutdown();
        m_Initialized = false;
        // Feature implementations may own RHI resources. Destroy them while the
        // device is still alive instead of relying on RenderSystem destruction.
        m_RenderPipeline.reset();
        m_Swapchain.reset();
        m_RenderDevice.reset();
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

    bool RenderSystem::setRenderPipeline(std::unique_ptr<IRenderPipeline> pipeline)
    {
        if (!pipeline)
            return false;
        m_RenderPipeline = std::move(pipeline);
        return true;
    }

    bool RenderSystem::registerRenderFeature(std::shared_ptr<IRenderFeature> feature)
    {
        return m_RenderPipeline && m_RenderPipeline->registerFeature(std::move(feature));
    }

    bool RenderSystem::unregisterRenderFeature(const std::string& name)
    {
        return m_RenderPipeline && m_RenderPipeline->unregisterFeature(name);
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

        if (m_RenderDevice)
        {
            BufferDesc vertex_desc{};
            vertex_desc.size = verts.size() * sizeof(MeshVertex);
            vertex_desc.usage = RhiBufferUsage::Vertex;
            vertex_desc.memory = RhiMemoryUsage::GPUOnly;
            vertex_desc.debug_name = "MeshGPU.Vertex";
            const auto vertex_result = m_RenderDevice->createBuffer(vertex_desc, verts.data(), vertex_desc.size);
            if (!vertex_result)
            {
                HBD_CORE_ERROR("{} rhi_mesh_vertex_create_failed asset={} reason={}",
                               kRenderSystemLogTag, id.value, vertex_result.error.message);
                return nullptr;
            }
            mgpu.rhi_vertex_buffer = vertex_result.value;

            BufferDesc index_desc{};
            index_desc.size = inds.size() * sizeof(uint32_t);
            index_desc.usage = RhiBufferUsage::Index;
            index_desc.memory = RhiMemoryUsage::GPUOnly;
            index_desc.debug_name = "MeshGPU.Index";
            const auto index_result = m_RenderDevice->createBuffer(index_desc, inds.data(), index_desc.size);
            if (!index_result)
            {
                (void)m_RenderDevice->destroyBuffer(mgpu.rhi_vertex_buffer);
                HBD_CORE_ERROR("{} rhi_mesh_index_create_failed asset={} reason={}",
                               kRenderSystemLogTag, id.value, index_result.error.message);
                return nullptr;
            }
            mgpu.rhi_index_buffer = index_result.value;
        }
        mgpu.submeshes = mesh->getSubmeshes();

        auto [it, inserted] = m_MeshCache.emplace(id, std::move(mgpu));
        return &it->second;
    }

    void RenderSystem::destroyMeshGPU(MeshGPU& mesh_gpu)
    {
        if (!m_RenderDevice)
            return;
        if (mesh_gpu.rhi_index_buffer)
            (void)m_RenderDevice->destroyBuffer(mesh_gpu.rhi_index_buffer);
        if (mesh_gpu.rhi_vertex_buffer)
            (void)m_RenderDevice->destroyBuffer(mesh_gpu.rhi_vertex_buffer);
        mesh_gpu.rhi_index_buffer = {};
        mesh_gpu.rhi_vertex_buffer = {};
    }

    void RenderSystem::clearMeshCache()
    {
        for (auto& [id, mesh_gpu] : m_MeshCache)
            destroyMeshGPU(mesh_gpu);
        m_MeshCache.clear();
    }

    bool RenderSystem::loadBuiltinShaders()
    {
        std::filesystem::path shader_root = std::filesystem::path(HYBRID_BINARY_ROOT_DIR) / "shader";
        if (!std::filesystem::exists(shader_root))
            shader_root = std::filesystem::path(HYBRID_PROJECT_ROOT_DIR) / "engine/shader";
        m_ShaderLibrary.setRoot(shader_root);

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
        if (!m_RhiFrameUniformBuffer && m_RenderDevice)
        {
            BufferDesc desc{};
            desc.size = sizeof(RU::FrameUBOData);
            desc.usage = RhiBufferUsage::Uniform;
            desc.memory = RhiMemoryUsage::CPUToGPU;
            desc.debug_name = "RenderSystem.FrameBlock";
            const auto result = m_RenderDevice->createBuffer(desc);
            if (!result)
                HBD_CORE_ERROR("{} rhi_ubo_create_failed block={} reason={}",
                               kRenderSystemLogTag, RU::kFrameBlockName, result.error.message);
            else
                m_RhiFrameUniformBuffer = result.value;
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
        if (m_RenderDevice && m_RhiFrameUniformBuffer)
        {
            const RhiStatus status = m_RenderDevice->updateBuffer(m_RhiFrameUniformBuffer, 0, &data, sizeof(data));
            if (!status)
                HBD_CORE_ERROR("{} rhi_ubo_upload_failed block={} reason={}",
                               kRenderSystemLogTag, RU::kFrameBlockName, status.error.message);
        }
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
            const auto result = Framebuffer::Create(spec, *m_RenderDevice);
            if (!result)
            {
                HBD_CORE_ERROR("{} framebuffer_create_failed reason={}",
                               kRenderSystemLogTag, result.error.message);
                return;
            }
            framebuffer = result.value;
        }
        else
        {
            framebuffer->resize(spec.width, spec.height);
        }
    }

    RenderSystem::ViewRenderTargets& RenderSystem::acquireViewTargets(RenderViewId id, const RenderViewRequest& view)
    {
        const uint32_t width = std::max(1u, static_cast<uint32_t>(view.size.x));
        const uint32_t height = std::max(1u, static_cast<uint32_t>(view.size.y));
        auto& targets = m_ViewTargets[id];
        ensureFramebuffer(targets.main, makeMainFramebufferSpec(width, height));
        if (HasFlag(view.flags, RenderFlags::SelectionHighlight))
            ensureFramebuffer(targets.selection, makeSelectionFramebufferSpec(width, height));
        else
            targets.selection.reset();
        targets.last_used_frame = m_RenderFrameIndex;
        return targets;
    }

    void RenderSystem::onWindowResize(uint32_t width, uint32_t height)
    {
        if (!m_Initialized)
            return;
        if (m_Swapchain)
        {
            const RhiStatus status = m_Swapchain->resize(width, height);
            if (!status)
                HBD_CORE_WARN("{} swapchain_resize_failed reason={}", kRenderSystemLogTag, status.error.message);
        }
    }

    RhiStatus RenderSystem::present()
    {
        if (!m_Initialized || !m_Swapchain)
            return RhiStatus::Failure(RhiErrorCode::InvalidState, "render system swapchain is unavailable");
        return m_Swapchain->present();
    }

    void RenderSystem::invalidateAsset(AssetID id, AssetType type)
    {
        if (id.value == 0)
            return;

        switch (type)
        {
        case AssetType::Mesh:
            if (const auto found = m_MeshCache.find(id); found != m_MeshCache.end())
            {
                destroyMeshGPU(found->second);
                m_MeshCache.erase(found);
            }
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
                                                 const RenderViewRequest* view_request,
                                                 bool cache_editor_camera_state)
    {
        std::shared_ptr<Scene> scene = frame_context.scene ? frame_context.scene : m_Scene;
        FrameViewResolveInput view_input{};
        view_input.scene = scene;
        view_input.frame = &frame_context;
        view_input.view_request = view_request;
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
        packet_input.view_request = view_request;
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

    void RenderSystem::renderFrameInternal(const FrameContext& frame_context,
                                           const RenderViewRequest& view,
                                           const ResolvedRenderTargets& view_targets)
    {
        const RenderFlags flags = view.flags;
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
                                                                     const RenderSelectionState* editor_selection,
                                                                     const RenderPostProcessState* post_process,
                                                                     RenderFlags current_flags,
                                                                     const ResolvedRenderTargets& targets)
        {
            RenderContextBuildInput context_input{};
            context_input.frame = &current_frame;
            context_input.packet = &packet;
            context_input.editor_selection = editor_selection;
            context_input.flags = current_flags;
            context_input.device = m_RenderDevice.get();
            context_input.frame_uniform_buffer = m_RhiFrameUniformBuffer;
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
            if (m_RenderPipeline)
                m_RenderPipeline->execute(context, make_pipeline_callbacks());
            const auto render_end = std::chrono::steady_clock::now();
            updateStatsFromPacket(packet, std::chrono::duration<float, std::milli>(render_end - render_begin).count());
        };

        if (!m_Initialized)
            return;

        if (flags == RenderFlags::None)
            return;

        if (frame_context.viewport_size.x <= 0.0f || frame_context.viewport_size.y <= 0.0f)
            return;

        auto packet = buildRenderPacket(frame_context, flags, &view,
                                        view.camera_source == RenderCameraSource::ExplicitMatrices);
        execute_render(frame_context,
                       packet,
                       &view.selection,
                       &view.post_process,
                       flags,
                       view_targets);
    }

    RenderFrameResult RenderSystem::renderFrame(const RenderFrameRequest& request)
    {
        ++m_RenderFrameIndex;
        RenderFrameResult result;
        result.views.reserve(request.views.size());

        for (const RenderViewRequest& view : request.views)
        {
            if (view.size.x <= 0.0f || view.size.y <= 0.0f)
                continue;

            FrameContext frame{};
            frame.dt = request.dt;
            frame.scene = request.scene;
            frame.input = request.input;
            frame.viewport_size = view.size;

            const RenderViewId view_id = resolveViewId(view);
            ViewRenderTargets& targets = acquireViewTargets(view_id, view);
            targets.graph_resources.clearExternalResources();
            if (targets.main)
            {
                targets.graph_resources.importExternalResource(
                    "SceneColor", targets.main->getColorAttachmentView(RenderTargets::kSceneColorAttachment));
                targets.graph_resources.importExternalResource(
                    "SceneEntityID", targets.main->getColorAttachmentView(RenderTargets::kSceneEntityIDAttachment));
                targets.graph_resources.importExternalResource("SceneDepth", targets.main->getDepthAttachmentView());
            }
            if (targets.selection)
            {
                targets.graph_resources.importExternalResource(
                    "SelectionMask", targets.selection->getColorAttachmentView(RenderTargets::kSelectionMaskAttachment));
                targets.graph_resources.importExternalResource("SelectionDepth", targets.selection->getDepthAttachmentView());
            }
            if (m_ShadowCascadeFBs[0])
                targets.graph_resources.importExternalResource("ShadowDepth", m_ShadowCascadeFBs[0]->getDepthAttachmentView());
            if (m_RenderPipeline && m_RenderDevice)
            {
                const RhiStatus materialize = targets.graph_resources.prepare(
                    *m_RenderDevice, m_RenderPipeline->getCompiledGraph(),
                    std::max(1u, static_cast<uint32_t>(view.size.x)),
                    std::max(1u, static_cast<uint32_t>(view.size.y)));
                if (!materialize)
                {
                    HBD_CORE_ERROR("{} graph_resource_materialization_failed view={} reason={}",
                                   kRenderSystemLogTag, view.name, materialize.error.message);
                    continue;
                }
            }
            ResolvedRenderTargets resolved{};
            resolved.framebuffer = targets.main;
            resolved.scene_framebuffer = targets.main;
            resolved.selection_framebuffer = targets.selection;
            resolved.shadow_framebuffer = m_ShadowCascadeFBs[0];
            resolved.shadow_cascade_framebuffers = &m_ShadowCascadeFBs;
            resolved.graph_resources = &targets.graph_resources;
            renderFrameInternal(frame, view, resolved);

            RenderViewResult view_result{};
            view_result.name = view.name;
            view_result.id = view_id;
            view_result.color_texture = targets.main
                ? targets.main->getColorAttachmentView(RenderTargets::kSceneColorAttachment)
                : TextureViewHandle{};
            if (view.picking && HasFlag(view.flags, RenderFlags::PickingID))
            {
                if (targets.main &&
                    view.picking->x >= 0 && view.picking->y >= 0 &&
                    view.picking->x < static_cast<int>(targets.main->getWidth()) &&
                    view.picking->y < static_cast<int>(targets.main->getHeight()))
                {
                    const uint32_t encoded = targets.main->readPixelUInt(
                        RenderTargets::kSceneEntityIDAttachment, view.picking->x, view.picking->y);
                    view_result.picked_entity = decodeEntityID(encoded);
                }
                else
                    view_result.picked_entity = kInvalidEntityID;
            }
            result.views.push_back(std::move(view_result));
        }

        for (auto it = m_ViewTargets.begin(); it != m_ViewTargets.end();)
        {
            if (m_RenderFrameIndex > it->second.last_used_frame + 120)
                it = m_ViewTargets.erase(it);
            else
                ++it;
        }
        return result;
    }

}
