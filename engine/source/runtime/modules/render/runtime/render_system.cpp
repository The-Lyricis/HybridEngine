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
#include "runtime/modules/render/runtime/render_bindings.h"
#include "runtime/modules/render/runtime/render_shaders.h"
#include "runtime/modules/render/runtime/render_targets.h"
#include "runtime/modules/scene/scene.h"
#include "runtime/modules/scene/components.h"
#include "runtime/core/base/intersection.h"
#include "runtime/core/base/macro.h"
#include "runtime/core/base/math_util.h"
#include "runtime/modules/asset/asset_manager.h"
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

        void computeLightSpaceBounds(const std::array<glm::vec3, 8>& corners,
                                     const glm::mat4& light_view,
                                     glm::vec3& out_min_ls,
                                     glm::vec3& out_max_ls)
        {
            out_min_ls = glm::vec3(std::numeric_limits<float>::max());
            out_max_ls = glm::vec3(std::numeric_limits<float>::lowest());
            for (const glm::vec3& corner : corners)
            {
                const glm::vec3 ls = glm::vec3(light_view * glm::vec4(corner, 1.0f));
                out_min_ls = glm::min(out_min_ls, ls);
                out_max_ls = glm::max(out_max_ls, ls);
            }
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

        bool sameDrawItemSubmesh(const RenderDrawItem& lhs, const RenderDrawItem& rhs)
        {
            return lhs.entityID == rhs.entityID &&
                   lhs.meshId.value == rhs.meshId.value &&
                   lhs.materialId.value == rhs.materialId.value &&
                   lhs.indexOffset == rhs.indexOffset &&
                   lhs.indexCount == rhs.indexCount;
        }

        static glm::vec3 lightDirectionFromTransform(const Hybrid::TransformComponent &tr)
        {
            const glm::vec3 dir = glm::vec3(tr.WorldMatrix * glm::vec4(0.0f, 0.0f, -1.0f, 0.0f));
            const float len = glm::length(dir);
            if (len < 1e-4f)
                return glm::vec3(0.0f, -1.0f, 0.0f);
            return dir / len;
        }

        uint32_t resolveActiveSelectionEntityID(const EditorRenderExt* editor_ext)
        {
            if (!editor_ext)
                return kInvalidEntityID;
            return editor_ext->selection.active_entity;
        }
        // Returns false if no valid camera found.
        static bool getSceneCameraMatrices(Hybrid::Scene& scene,
            float aspect,
            glm::mat4& outView,
            glm::mat4& outProj,
            glm::vec3& outCamPos,
            glm::vec4* outClearColor = nullptr,
            bool* outUseSkyboxClear = nullptr)
        {
            auto& reg = scene.getRegistry();
            auto view = reg.view<Hybrid::TransformComponent, Hybrid::CameraComponent>();

            entt::entity mainCam = entt::null;
            for (auto e : view)
            {
                auto& cam = view.get<Hybrid::CameraComponent>(e);
                if (!cam.Enabled)
                    continue;
                if (cam.Primary) { mainCam = e; break; }
            }
            if (mainCam == entt::null)
            {
                for (auto e : view)
                {
                    auto& cam = view.get<Hybrid::CameraComponent>(e);
                    if (cam.Enabled)
                    {
                        mainCam = e;
                        break;
                    }
                }
            }
            if (mainCam == entt::null) return false;

            const auto& tr = reg.get<Hybrid::TransformComponent>(mainCam);
            const auto& cam = reg.get<Hybrid::CameraComponent>(mainCam);

            outProj = glm::perspective(glm::radians(cam.FovY), aspect, cam.Near, cam.Far);

            outView = glm::inverse(tr.WorldMatrix);

            outCamPos = glm::vec3(tr.WorldMatrix[3]);
            if (outClearColor)
                *outClearColor = cam.ClearColor;
            if (outUseSkyboxClear)
                *outUseSkyboxClear = (cam.ClearMode == CameraClearMode::Skybox);
            return true;
        }
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
            m_SceneShader->setUniformBlockBinding(RU::kFrameBlockName, RU::kFrameUBOBinding);
            m_SceneShader->setUniformBlockBinding(RU::kLightBlockName, RU::kLightUBOBinding);
            m_SceneShader->setInt(RenderBindings::kSceneAlbedoUniform, RenderBindings::kSceneAlbedoSlot);
            m_SceneShader->setInt(RenderBindings::kSceneNormalUniform, RenderBindings::kSceneNormalSlot);
            m_SceneShader->setInt(RenderBindings::kSceneMRUniform, RenderBindings::kSceneMRSlot);
            m_SceneShader->setInt(RenderBindings::kSceneAOUniform, RenderBindings::kSceneAOSlot);
            m_SceneShader->setInt(RenderBindings::kSceneEmissiveUniform, RenderBindings::kSceneEmissiveSlot);
            for (uint32_t cascade_index = 0; cascade_index < kMaxDirectionalShadowCascades; ++cascade_index)
            {
                m_SceneShader->setInt(std::string(RenderBindings::kSceneShadowMapUniform) + "[" + std::to_string(cascade_index) + "]",
                                      static_cast<int>(RenderBindings::kSceneShadowMapSlot + cascade_index));
            }
        }

        if (auto selection_mask_shader = m_ShaderLibrary.get(std::string(RenderShaders::kSelectionMask.name)))
        {
            selection_mask_shader->bind();
            selection_mask_shader->setUniformBlockBinding(RU::kFrameBlockName, RU::kFrameUBOBinding);
            selection_mask_shader->setInt(RenderBindings::kSceneAlbedoUniform, RenderBindings::kSceneAlbedoSlot);
        }

        if (m_SkyboxShader)
        {
            m_SkyboxShader->bind();
            m_SkyboxShader->setUniformBlockBinding(RU::kFrameBlockName, RU::kFrameUBOBinding);
            m_SkyboxShader->setInt(RenderBindings::kSkyboxCubemapUniform, RenderBindings::kSkyboxCubemapSlot);
        }

        if (m_ShadowShader)
        {
            m_ShadowShader->bind();
            m_ShadowShader->setInt(RenderBindings::kSceneAlbedoUniform, RenderBindings::kSceneAlbedoSlot);
        }

        if (auto selection_overlay_shader = m_ShaderLibrary.get(std::string(RenderShaders::kSelectionOverlay.name)))
        {
            selection_overlay_shader->bind();
            selection_overlay_shader->setInt(RenderBindings::kSelectionOverlaySceneColorUniform,
                                             RenderBindings::kSelectionOverlaySceneColorSlot);
            selection_overlay_shader->setInt(RenderBindings::kSelectionOverlaySceneDepthUniform,
                                             RenderBindings::kSelectionOverlaySceneDepthSlot);
            selection_overlay_shader->setInt(RenderBindings::kSelectionOverlayMaskUniform,
                                             RenderBindings::kSelectionOverlayMaskSlot);
            selection_overlay_shader->setInt(RenderBindings::kSelectionOverlaySelectedDepthUniform,
                                             RenderBindings::kSelectionOverlaySelectedDepthSlot);
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

    void RenderSystem::prepareShadowData(RenderPacket& packet, RenderFlags flags) const
    {
        packet.shadow = {};

        if (!HasFlag(flags, RenderFlags::Shadow))
            return;

        if (packet.lights.dir.intensity <= 0.0f)
            return;

        const glm::vec3 light_dir = MathUtil::normalize(packet.lights.dir.direction, glm::vec3(0.0f, -1.0f, 0.0f));

        glm::vec3 up(0.0f, 1.0f, 0.0f);
        if (std::abs(glm::dot(up, light_dir)) > 0.98f)
            up = glm::vec3(0.0f, 0.0f, 1.0f);

        packet.shadow.enabled = true;
        packet.shadow.lightDirection = light_dir;
        packet.shadow.strength = m_DirectionalShadowSettings.strength;
        packet.shadow.biasConstant = m_DirectionalShadowSettings.bias_constant;
        packet.shadow.biasSlope = m_DirectionalShadowSettings.bias_slope;

        const uint32_t cascade_count =
            std::clamp(m_DirectionalShadowSettings.cascade_count, 1u, kMaxDirectionalShadowCascades);
        packet.shadow.cascadeCount = cascade_count;

        float cascade_near = m_DirectionalShadowSettings.near_distance;
        for (uint32_t cascade_index = 0; cascade_index < cascade_count; ++cascade_index)
        {
            const float split_ratio =
                std::clamp(m_DirectionalShadowSettings.cascade_split_ratios[cascade_index], 0.0f, 1.0f);
            float cascade_far = m_DirectionalShadowSettings.near_distance +
                                (m_DirectionalShadowSettings.far_distance - m_DirectionalShadowSettings.near_distance) * split_ratio;
            if (cascade_index == cascade_count - 1)
                cascade_far = m_DirectionalShadowSettings.far_distance;
            cascade_far = std::max(cascade_far, cascade_near + 0.01f);

            const auto frustum_corners = buildCameraFrustumSliceCorners(packet.frame.view,
                                                                        packet.frame.proj,
                                                                        cascade_near,
                                                                        cascade_far);
            std::array<glm::vec3, 8> extruded_corners = frustum_corners;
            for (glm::vec3& corner : extruded_corners)
                corner -= light_dir * m_DirectionalShadowSettings.caster_back_padding;

            glm::vec3 frustum_center(0.0f);
            for (const glm::vec3& corner : frustum_corners)
                frustum_center += corner;
            frustum_center /= static_cast<float>(frustum_corners.size());

            const float light_distance = m_DirectionalShadowSettings.light_distance;
            const glm::vec3 eye = frustum_center - light_dir * light_distance;
            glm::mat4 light_view = glm::lookAt(eye, frustum_center, up);

            const glm::vec3 receiver_center_ls = glm::vec3(light_view * glm::vec4(frustum_center, 1.0f));
            float receiver_radius_ls = 0.0f;
            for (const glm::vec3& corner : frustum_corners)
            {
                const glm::vec3 corner_ls = glm::vec3(light_view * glm::vec4(corner, 1.0f));
                const glm::vec2 delta = glm::vec2(corner_ls) - glm::vec2(receiver_center_ls);
                receiver_radius_ls = std::max(receiver_radius_ls, std::max(std::abs(delta.x), std::abs(delta.y)));
            }

            receiver_radius_ls += m_DirectionalShadowSettings.receiver_margin_xy;
            receiver_radius_ls += m_DirectionalShadowSettings.projection_margin_xy;
            receiver_radius_ls = std::max(receiver_radius_ls, 0.5f);

            const float ortho_extent = receiver_radius_ls * 2.0f;
            const float texel_size = ortho_extent / static_cast<float>(m_DirectionalShadowSettings.map_resolution);

            glm::vec3 snapped_center_ls = receiver_center_ls;
            snapped_center_ls.x = std::round(snapped_center_ls.x / texel_size) * texel_size;
            snapped_center_ls.y = std::round(snapped_center_ls.y / texel_size) * texel_size;

            const glm::vec3 snap_offset_ls = snapped_center_ls - receiver_center_ls;
            light_view = glm::translate(glm::mat4(1.0f), glm::vec3(snap_offset_ls.x, snap_offset_ls.y, 0.0f)) * light_view;

            glm::vec3 receiver_min_ls{};
            glm::vec3 receiver_max_ls{};
            computeLightSpaceBounds(frustum_corners, light_view, receiver_min_ls, receiver_max_ls);

            glm::vec3 snapped_receiver_min_ls(receiver_min_ls);
            glm::vec3 snapped_receiver_max_ls(receiver_max_ls);
            snapped_receiver_min_ls.x = snapped_center_ls.x - receiver_radius_ls;
            snapped_receiver_max_ls.x = snapped_center_ls.x + receiver_radius_ls;
            snapped_receiver_min_ls.y = snapped_center_ls.y - receiver_radius_ls;
            snapped_receiver_max_ls.y = snapped_center_ls.y + receiver_radius_ls;

            glm::vec3 extruded_min_ls{};
            glm::vec3 extruded_max_ls{};
            computeLightSpaceBounds(extruded_corners, light_view, extruded_min_ls, extruded_max_ls);

            glm::vec3 snapped_caster_min_ls = glm::min(snapped_receiver_min_ls, extruded_min_ls);
            glm::vec3 snapped_caster_max_ls = glm::max(snapped_receiver_max_ls, extruded_max_ls);
            snapped_caster_max_ls.z += m_DirectionalShadowSettings.receiver_front_padding;

            const glm::mat4 light_proj = glm::ortho(snapped_receiver_min_ls.x,
                                                    snapped_receiver_max_ls.x,
                                                    snapped_receiver_min_ls.y,
                                                    snapped_receiver_max_ls.y,
                                                    -snapped_caster_max_ls.z,
                                                    -snapped_caster_min_ls.z);
            auto& cascade = packet.shadow.cascades[cascade_index];
            cascade.valid = true;
            cascade.lightViewProjection = light_proj * light_view;
            cascade.receiverMinLS = snapped_receiver_min_ls;
            cascade.receiverMaxLS = snapped_receiver_max_ls;
            cascade.casterMinLS = snapped_caster_min_ls;
            cascade.casterMaxLS = snapped_caster_max_ls;
            cascade.receiverCornersWS = frustum_corners;
            cascade.casterExtrudedCornersWS = extruded_corners;
            cascade.splitNear = cascade_near;
            cascade.splitFar = cascade_far;

            cascade_near = cascade_far;
        }
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
        RenderPacket pkt;

        const glm::vec2 viewport_size = frame_context.viewport_size;

        std::shared_ptr<Scene> scene = frame_context.scene ? frame_context.scene : m_Scene;

        bool use_game_camera = true;
        if (editor_ext)
        {
            use_game_camera = editor_ext->use_game_camera;
        }

        // A) resolve active camera (scene game camera or editor-provided camera).
        glm::mat4 viewM(1.0f), projM(1.0f);
        glm::vec3 cameraPos(0.0f, 0.0f, 3.0f);
        const float aspect = (viewport_size.y > 0.0f) ? (viewport_size.x / viewport_size.y) : 1.0f;
        bool has_camera = false;

        pkt.frame.clearColor = glm::vec4(0.1f, 0.1f, 0.12f, 1.0f);
        pkt.frame.useSkyboxClear = false;

        if (use_game_camera && scene)
        {
            has_camera = getSceneCameraMatrices(
                *scene,
                aspect,
                viewM,
                projM,
                cameraPos,
                &pkt.frame.clearColor,
                &pkt.frame.useSkyboxClear);
        }
        else if (!use_game_camera && editor_ext && editor_ext->has_editor_camera)
        {
            viewM = editor_ext->editor_view;
            projM = editor_ext->editor_proj;
            cameraPos = editor_ext->editor_camera_pos;
            has_camera = true;
            if (scene && scene->environment().skybox_cubemap.value != 0)
                pkt.frame.useSkyboxClear = true;
        }

        if (!has_camera && scene)
        {
            has_camera = getSceneCameraMatrices(*scene, aspect, viewM, projM, cameraPos);
        }

        if (!has_camera)
        {
            viewM = glm::mat4(1.0f);
            projM = glm::mat4(1.0f);
            cameraPos = glm::vec3(0.0f, 0.0f, 3.0f);
        }

        pkt.scene = scene;
        pkt.showColliderDebug = editor_ext ? editor_ext->show_collider_debug : false;
        pkt.showShadowDebug = editor_ext ? editor_ext->show_shadow_debug : false;
        pkt.activeEntityID = resolveActiveSelectionEntityID(editor_ext);
        if (scene)
        {
            const SceneEnvironmentSettings& environment = scene->environment();
            pkt.environment.skyboxCubemap = environment.skybox_cubemap;
            pkt.environment.skyboxIntensity = environment.skybox_intensity;
            pkt.environment.skyboxRotationDegrees = environment.skybox_rotation_degrees;
            if (environment.skybox_cubemap.value != 0)
                pkt.environment.skyboxTexture = getOrCreateCubemapTexture(environment.skybox_cubemap);
        }
        if (pkt.frame.useSkyboxClear && !pkt.environment.skyboxTexture)
            pkt.environment.skyboxTexture = getOrCreateCubemapTexture({});

        // B) write packet
        pkt.frame.view = viewM;
        pkt.frame.proj = projM;
        pkt.frame.viewProj = projM * viewM;
        pkt.frame.cameraPos = cameraPos;
        pkt.frame.time = frame_context.dt;
        if (editor_ext && editor_ext->game_viewport_size.x > 1.0f && editor_ext->game_viewport_size.y > 1.0f)
            pkt.frame.gameAspect = editor_ext->game_viewport_size.x / editor_ext->game_viewport_size.y;
        else
            pkt.frame.gameAspect = (aspect > 0.0f) ? aspect : (16.0f / 9.0f);

        if (cache_editor_camera_state)
        {
            m_LastView = viewM;
            m_LastProj = projM;
        }

        collectPacketLights(pkt);
        prepareShadowData(pkt, flags);
        const Frustum frustum = has_camera ? BuildFrustum(pkt.frame.viewProj) : Frustum{};
        collectPacketDrawItems(pkt, frustum);
        collectShadowCasterItems(pkt);
        sortRenderPacket(pkt);

        return pkt;
    }

    void RenderSystem::collectPacketLights(RenderPacket& packet) const
    {
        packet.lights.dir.intensity = 0.0f;
        packet.lights.points.clear();
        packet.lights.points.reserve(RU::kMaxPointLights);

        if (!packet.scene)
            return;

        auto& registry = packet.scene->getRegistry();

        auto dir_view = registry.view<Hybrid::TransformComponent, Hybrid::DirectionalLightComponent>();
        for (auto entity : dir_view)
        {
            const auto& transform = dir_view.get<Hybrid::TransformComponent>(entity);
            const auto& light = dir_view.get<Hybrid::DirectionalLightComponent>(entity);
            if (!light.Enabled)
                continue;

            packet.lights.dir.color = light.Color;
            packet.lights.dir.intensity = light.Intensity;
            packet.lights.dir.direction = lightDirectionFromTransform(transform);
            break;
        }

        auto point_view = registry.view<Hybrid::TransformComponent, Hybrid::PointLightComponent>();
        for (auto entity : point_view)
        {
            if (static_cast<int>(packet.lights.points.size()) >= RU::kMaxPointLights)
                break;

            const auto& transform = point_view.get<Hybrid::TransformComponent>(entity);
            const auto& light = point_view.get<Hybrid::PointLightComponent>(entity);
            if (!light.Enabled)
                continue;

            RenderPointLightData point{};
            point.color = light.Color;
            point.intensity = light.Intensity;
            point.position = transform.Position;
            point.range = light.Range;
            packet.lights.points.push_back(point);
        }
    }

    void RenderSystem::collectItemsForFrustum(RenderPacket& packet,
                                              const Frustum& frustum,
                                              std::vector<RenderDrawItem>* opaque_items,
                                              std::vector<RenderDrawItem>* transparent_items,
                                              std::vector<RenderDrawItem>* shadow_items,
                                              uint32_t* tested_items,
                                              uint32_t* culled_items,
                                              bool count_scene_totals)
    {
        if (!packet.scene || !m_AssetManager)
            return;

        auto& registry = packet.scene->getRegistry();
        auto render_view = registry.view<Hybrid::TransformComponent, Hybrid::MeshRendererComponent>();

        for (auto entity : render_view)
        {
            const auto& transform = render_view.get<Hybrid::TransformComponent>(entity);
            const auto& renderer = render_view.get<Hybrid::MeshRendererComponent>(entity);
            if (!renderer.Enabled || renderer.Mesh.value == 0)
                continue;

            if (count_scene_totals)
                ++m_Stats.scene_renderers;

            std::shared_ptr<Mesh> cpu_mesh = m_AssetManager->loadSync<Mesh>(renderer.Mesh);
            if (!cpu_mesh)
                continue;

            MeshGPU* mesh_gpu = getOrCreateMeshGPU(renderer.Mesh, cpu_mesh);
            if (!mesh_gpu)
                continue;

            std::shared_ptr<Material> base_material;
            AssetID base_material_id = renderer.Material;
            if (base_material_id.value != 0)
                base_material = m_AssetManager->loadSync<Material>(base_material_id);

            if (!base_material && !mesh_gpu->submeshes.empty() && mesh_gpu->submeshes[0].material.value != 0)
            {
                base_material_id = mesh_gpu->submeshes[0].material;
                base_material = m_AssetManager->loadSync<Material>(base_material_id);
            }

            if (!base_material)
            {
                base_material = m_AssetManager->getDefault<Material>();
                base_material_id = AssetID{};
            }

            auto* base_material_gpu = m_MaterialSystem.getOrCreate(base_material_id, base_material);
            if (!base_material_gpu)
                continue;

            for (const auto& submesh : mesh_gpu->submeshes)
            {
                if (count_scene_totals)
                    ++m_Stats.scene_submeshes;
                if (tested_items)
                    ++(*tested_items);

                const AABB local_bounds{submesh.aabb_min, submesh.aabb_max};
                const AABB world_bounds = TransformAABB(local_bounds, transform.WorldMatrix);
                if (!IntersectsFrustum(frustum, world_bounds))
                {
                    if (culled_items)
                        ++(*culled_items);
                    continue;
                }

                AssetID effective_material_id = base_material_id;
                const MaterialSystem::MaterialGPU* effective_material_gpu = base_material_gpu;

                if (submesh.material.value != 0)
                {
                    auto sub_material = m_AssetManager->loadSync<Material>(submesh.material);
                    if (sub_material)
                    {
                        if (auto* sub_material_gpu = m_MaterialSystem.getOrCreate(submesh.material, sub_material))
                        {
                            effective_material_id = submesh.material;
                            effective_material_gpu = sub_material_gpu;
                        }
                    }
                }

                RenderDrawItem item{};
                item.meshId = renderer.Mesh;
                item.materialId = effective_material_id;
                item.meshGPU = mesh_gpu;
                item.materialGPU = effective_material_gpu;
                item.indexOffset = submesh.index_offset;
                item.indexCount = submesh.index_count;
                item.model = transform.WorldMatrix;
                item.tint = renderer.Tint;
                item.entityID = static_cast<uint32_t>(entt::to_integral(entity));

                const MaterialSurfaceMode surface_mode = effective_material_gpu->data.surface_mode;

                if (shadow_items)
                {
                    if (surface_mode != MaterialSurfaceMode::Transparent)
                        shadow_items->push_back(item);
                    continue;
                }

                if (surface_mode == MaterialSurfaceMode::Transparent)
                {
                    if (transparent_items)
                        transparent_items->push_back(item);
                }
                else
                {
                    if (opaque_items)
                        opaque_items->push_back(item);
                }
            }
        }
    }

    void RenderSystem::collectPacketDrawItems(RenderPacket& packet, const Frustum& frustum)
    {
        packet.opaque_items.clear();
        packet.transparent_items.clear();
        packet.tested_items = 0;
        packet.culled_items = 0;

        m_Stats.scene_renderers = 0;
        m_Stats.scene_submeshes = 0;

        if (packet.scene)
        {
            auto& registry = packet.scene->getRegistry();
            auto render_view = registry.view<Hybrid::TransformComponent, Hybrid::MeshRendererComponent>();
            packet.opaque_items.reserve(render_view.size_hint());
            packet.transparent_items.reserve(render_view.size_hint() / 4);
        }

        collectItemsForFrustum(packet,
                               frustum,
                               &packet.opaque_items,
                               &packet.transparent_items,
                               nullptr,
                               &packet.tested_items,
                               &packet.culled_items,
                               true);
    }

    void RenderSystem::collectItemsForVolume(RenderPacket& packet,
                                             const ConvexVolume& volume,
                                             std::vector<RenderDrawItem>* shadow_items)
    {
        if (!packet.scene || !m_AssetManager || !shadow_items || !volume.Valid)
            return;

        auto& registry = packet.scene->getRegistry();
        auto render_view = registry.view<Hybrid::TransformComponent, Hybrid::MeshRendererComponent>();

        for (auto entity : render_view)
        {
            const auto& transform = render_view.get<Hybrid::TransformComponent>(entity);
            const auto& renderer = render_view.get<Hybrid::MeshRendererComponent>(entity);
            if (!renderer.Enabled || renderer.Mesh.value == 0)
                continue;

            std::shared_ptr<Mesh> cpu_mesh = m_AssetManager->loadSync<Mesh>(renderer.Mesh);
            if (!cpu_mesh)
                continue;

            MeshGPU* mesh_gpu = getOrCreateMeshGPU(renderer.Mesh, cpu_mesh);
            if (!mesh_gpu)
                continue;

            std::shared_ptr<Material> base_material;
            AssetID base_material_id = renderer.Material;
            if (base_material_id.value != 0)
                base_material = m_AssetManager->loadSync<Material>(base_material_id);

            if (!base_material && !mesh_gpu->submeshes.empty() && mesh_gpu->submeshes[0].material.value != 0)
            {
                base_material_id = mesh_gpu->submeshes[0].material;
                base_material = m_AssetManager->loadSync<Material>(base_material_id);
            }

            if (!base_material)
            {
                base_material = m_AssetManager->getDefault<Material>();
                base_material_id = AssetID{};
            }

            auto* base_material_gpu = m_MaterialSystem.getOrCreate(base_material_id, base_material);
            if (!base_material_gpu)
                continue;

            for (const auto& submesh : mesh_gpu->submeshes)
            {
                const AABB local_bounds{submesh.aabb_min, submesh.aabb_max};
                const AABB world_bounds = TransformAABB(local_bounds, transform.WorldMatrix);
                if (!IntersectsConvexVolume(volume, world_bounds))
                    continue;

                AssetID effective_material_id = base_material_id;
                const MaterialSystem::MaterialGPU* effective_material_gpu = base_material_gpu;

                if (submesh.material.value != 0)
                {
                    auto sub_material = m_AssetManager->loadSync<Material>(submesh.material);
                    if (sub_material)
                    {
                        if (auto* sub_material_gpu = m_MaterialSystem.getOrCreate(submesh.material, sub_material))
                        {
                            effective_material_id = submesh.material;
                            effective_material_gpu = sub_material_gpu;
                        }
                    }
                }

                if (effective_material_gpu->data.surface_mode == MaterialSurfaceMode::Transparent)
                    continue;

                RenderDrawItem item{};
                item.meshId = renderer.Mesh;
                item.materialId = effective_material_id;
                item.meshGPU = mesh_gpu;
                item.materialGPU = effective_material_gpu;
                item.indexOffset = submesh.index_offset;
                item.indexCount = submesh.index_count;
                item.model = transform.WorldMatrix;
                item.tint = renderer.Tint;
                item.entityID = static_cast<uint32_t>(entt::to_integral(entity));
                shadow_items->push_back(item);
            }
        }
    }

    void RenderSystem::collectShadowCasterItems(RenderPacket& packet)
    {
        packet.shadow_caster_items.clear();

        if (!packet.shadow.enabled)
            return;

        std::vector<RenderDrawItem> gathered_items;
        for (uint32_t cascade_index = 0; cascade_index < packet.shadow.cascadeCount; ++cascade_index)
        {
            const auto& cascade = packet.shadow.cascades[cascade_index];
            if (!cascade.valid)
                continue;

            std::array<glm::vec3, 16> hull_points{};
            for (size_t i = 0; i < 8; ++i)
            {
                hull_points[i] = cascade.receiverCornersWS[i];
                hull_points[8 + i] = cascade.casterExtrudedCornersWS[i];
            }

            const ConvexVolume caster_volume = BuildConvexHullVolume(hull_points);
            if (caster_volume.Valid)
                collectItemsForVolume(packet, caster_volume, &gathered_items);
        }

        packet.shadow_caster_items = std::move(gathered_items);
        auto& items = packet.shadow_caster_items;
        std::sort(items.begin(), items.end(),
                  [](const RenderDrawItem& lhs, const RenderDrawItem& rhs)
                  {
                      if (lhs.materialId.value != rhs.materialId.value)
                          return lhs.materialId.value < rhs.materialId.value;
                      if (lhs.meshId.value != rhs.meshId.value)
                          return lhs.meshId.value < rhs.meshId.value;
                      if (lhs.entityID != rhs.entityID)
                          return lhs.entityID < rhs.entityID;
                      if (lhs.indexOffset != rhs.indexOffset)
                          return lhs.indexOffset < rhs.indexOffset;
                      return lhs.indexCount < rhs.indexCount;
                  });
        items.erase(std::unique(items.begin(), items.end(), sameDrawItemSubmesh), items.end());
    }

    void RenderSystem::sortRenderPacket(RenderPacket& packet) const
    {
        auto opaque_less = [](const RenderDrawItem& lhs, const RenderDrawItem& rhs)
        {
            if (lhs.materialId.value != rhs.materialId.value)
                return lhs.materialId.value < rhs.materialId.value;
            if (lhs.meshId.value != rhs.meshId.value)
                return lhs.meshId.value < rhs.meshId.value;
            return lhs.entityID < rhs.entityID;
        };

        std::sort(packet.opaque_items.begin(), packet.opaque_items.end(), opaque_less);

        std::sort(packet.transparent_items.begin(),
                  packet.transparent_items.end(),
                  [&packet](const RenderDrawItem& lhs, const RenderDrawItem& rhs)
                  {
                      const glm::vec3 camera_pos = packet.frame.cameraPos;
                      const glm::vec3 lhs_pos = glm::vec3(lhs.model[3]);
                      const glm::vec3 rhs_pos = glm::vec3(rhs.model[3]);
                      const float lhs_dist2 = glm::dot(lhs_pos - camera_pos, lhs_pos - camera_pos);
                      const float rhs_dist2 = glm::dot(rhs_pos - camera_pos, rhs_pos - camera_pos);
                      if (lhs_dist2 != rhs_dist2)
                          return lhs_dist2 > rhs_dist2;
                      return lhs.entityID < rhs.entityID;
                  });

        std::sort(packet.shadow_caster_items.begin(), packet.shadow_caster_items.end(), opaque_less);
    }

    void RenderSystem::updateStatsFromPacket(const RenderPacket& packet, float render_cpu_time_ms)
    {
        m_Stats.render_cpu_time_ms = std::max(0.0f, render_cpu_time_ms);
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
            callbacks.gizmo = [this](RenderContext& context)
            {
                m_GizmoPass.execute(context);
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
                RenderContext scene_context{};
                scene_context.frame = &scene_frame;
                scene_context.packet = &scene_packet;
                scene_context.editor_selection = &scene_ext.selection;
                scene_context.flags = flags;
                scene_context.window_handle = window_handle;
                scene_context.framebuffer = m_SceneFB;
                scene_context.scene_framebuffer = m_SceneFB;
                scene_context.selection_framebuffer = m_SelectionFB;
                scene_context.shadow_framebuffer = m_ShadowCascadeFBs[0];
                scene_context.shadow_cascade_framebuffers = &m_ShadowCascadeFBs;
                scene_context.selection_overlay_style = &m_SelectionOverlayStyle;
                scene_context.shader_library = &m_ShaderLibrary;
                scene_context.scene_shader = m_SceneShader;
                scene_context.skybox_shader = m_SkyboxShader;
                scene_context.shadow_shader = m_ShadowShader;
                scene_context.collider_debug_shader = m_ColliderDebugShader;
                updateFrameUBO(scene_packet, scene_frame.viewport_size);
                updateLightUBO(scene_packet);
                const auto render_begin = std::chrono::steady_clock::now();
                m_RenderPipeline.execute(scene_context, make_pipeline_callbacks());
                const auto render_end = std::chrono::steady_clock::now();
                updateStatsFromPacket(
                    scene_packet,
                    std::chrono::duration<float, std::milli>(render_end - render_begin).count());
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
                RenderContext game_context{};
                game_context.frame = &game_frame;
                game_context.packet = &game_packet;
                game_context.editor_selection = &game_ext.selection;
                game_context.flags = game_flags;
                game_context.window_handle = window_handle;
                game_context.framebuffer = m_GameFB;
                game_context.scene_framebuffer = m_GameFB;
                game_context.selection_framebuffer = nullptr;
                game_context.shadow_framebuffer = m_ShadowCascadeFBs[0];
                game_context.shadow_cascade_framebuffers = &m_ShadowCascadeFBs;
                game_context.selection_overlay_style = &m_SelectionOverlayStyle;
                game_context.shader_library = &m_ShaderLibrary;
                game_context.scene_shader = m_SceneShader;
                game_context.skybox_shader = m_SkyboxShader;
                game_context.shadow_shader = m_ShadowShader;
                game_context.collider_debug_shader = m_ColliderDebugShader;
                updateFrameUBO(game_packet, game_frame.viewport_size);
                updateLightUBO(game_packet);
                const auto render_begin = std::chrono::steady_clock::now();
                m_RenderPipeline.execute(game_context, make_pipeline_callbacks());
                const auto render_end = std::chrono::steady_clock::now();
                updateStatsFromPacket(
                    game_packet,
                    std::chrono::duration<float, std::milli>(render_end - render_begin).count());
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
        RenderContext context{};
        context.frame = &frame_context;
        context.packet = &packet;
        context.editor_selection = editor_ext ? &editor_ext->selection : nullptr;
        context.flags = flags;
        context.window_handle = window_handle;
        context.framebuffer = m_SceneFB;
        context.scene_framebuffer = m_SceneFB;
        context.selection_framebuffer = m_SelectionFB;
        context.shadow_framebuffer = m_ShadowCascadeFBs[0];
        context.shadow_cascade_framebuffers = &m_ShadowCascadeFBs;
        context.selection_overlay_style = &m_SelectionOverlayStyle;
        context.shader_library = &m_ShaderLibrary;
        context.scene_shader = m_SceneShader;
        context.skybox_shader = m_SkyboxShader;
        context.shadow_shader = m_ShadowShader;
        context.collider_debug_shader = m_ColliderDebugShader;
        updateFrameUBO(packet, frame_context.viewport_size);
        updateLightUBO(packet);
        const auto render_begin = std::chrono::steady_clock::now();
        m_RenderPipeline.execute(context, make_pipeline_callbacks());
        const auto render_end = std::chrono::steady_clock::now();
        updateStatsFromPacket(
            packet,
            std::chrono::duration<float, std::milli>(render_end - render_begin).count());
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
