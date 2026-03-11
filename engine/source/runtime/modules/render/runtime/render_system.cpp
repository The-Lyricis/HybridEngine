#include "render_system.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <glad/gl.h>

#include <algorithm>
#include <cstddef>
#include <array>
#include <filesystem>
#include <string>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include "runtime/modules/render/public/renderer.h"
#include "runtime/modules/render/public/render_command.h"
#include "runtime/modules/render/public/vertex_array.h"
#include "runtime/modules/render/public/shader.h"
#include "runtime/modules/render/public/framebuffer.h"
#include "runtime/modules/render/public/texture.h"
#include "runtime/modules/scene/scene.h"
#include "runtime/modules/scene/components.h"
#include "runtime/core/base/macro.h"
#include "runtime/core/base/math_util.h"
#include "runtime/modules/asset/asset_manager.h"
#include "runtime/modules/asset/material.h"
#include "runtime/modules/asset/mesh.h"
#include <runtime/modules/scene/components/collider_component.h>


namespace Hybrid
{

    namespace
    {
        uint32_t encodeEntityID(uint32_t entity_id)
        {
            return entity_id + 1u;
        }

        uint32_t decodeEntityID(uint32_t encoded_id)
        {
            return (encoded_id == 0) ? kInvalidEntityID : (encoded_id - 1u);
        }

        void setSceneFramebufferDrawBuffers(bool write_entity_id)
        {
            if (write_entity_id)
            {
                constexpr GLenum buffers[2] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
                glDrawBuffers(2, buffers);
            }
            else
            {
                constexpr GLenum buffer = GL_COLOR_ATTACHMENT0;
                glDrawBuffers(1, &buffer);
            }
        }

        static glm::vec3 lightDirectionFromTransform(const Hybrid::TransformComponent &tr)
        {
            const glm::vec3 dir = glm::vec3(tr.WorldMatrix * glm::vec4(0.0f, 0.0f, -1.0f, 0.0f));
            const float len = glm::length(dir);
            if (len < 1e-4f)
                return glm::vec3(0.0f, -1.0f, 0.0f);
            return dir / len;
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
    }

    void RenderSystem::initialize(void *glfwWindowHandle)
    {
        if (m_Initialized)
            return;

        Renderer::initialize();
        if (!loadBuiltinShaders())
        {
            HBD_CORE_ERROR("RenderSystem failed to load builtin shaders");
            return;
        }

        GLFWwindow *window = static_cast<GLFWwindow *>(glfwWindowHandle);
        int w = 0, h = 0;
        glfwGetFramebufferSize(window, &w, &h);

        FramebufferSpec spec;
        spec.width = (uint32_t)std::max(1, w);
        spec.height = (uint32_t)std::max(1, h);
        m_SceneFB = Framebuffer::Create(spec);
        m_GameFB = Framebuffer::Create(spec);

        m_Initialized = true;

        HBD_CORE_TRACE("RenderSystem initialized");
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

        m_MeshShader = m_ShaderLibrary.get("HybridDefault");
        m_DebugBoxShader = m_ShaderLibrary.get("BoxColider");
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

        const bool mesh_ok = m_ShaderLibrary.load(
            "HybridDefault",
            "HybridDefault.vert",
            "HybridDefault.frag");
        const bool debug_box_ok = m_ShaderLibrary.load(
            "BoxColider",
            "BoxColider.vert",
            "BoxColider.frag");
        const bool selection_outline_ok = m_ShaderLibrary.load(
            "SelectionOutline",
            "SelectionOutline.vert",
            "SelectionOutline.frag");

        m_MeshShader = m_ShaderLibrary.get("HybridDefault");
        m_DebugBoxShader = m_ShaderLibrary.get("BoxColider");
        return mesh_ok && debug_box_ok && selection_outline_ok && m_MeshShader && m_DebugBoxShader;
    }

    void RenderSystem::ensureFramebufferSize(std::shared_ptr<Framebuffer>& framebuffer, uint32_t w, uint32_t h)
    {
        w = std::max(1u, w);
        h = std::max(1u, h);

        if (!framebuffer)
        {
            FramebufferSpec spec{w, h};
            framebuffer = Framebuffer::Create(spec);
        }
        else
        {
            framebuffer->resize(w, h);
        }
    }

    uint32_t RenderSystem::getSceneColorTexture() const
    {
        return m_SceneFB ? m_SceneFB->getColorAttachmentRendererID() : 0;
    }

    uint32_t RenderSystem::getGameColorTexture() const
    {
        return m_GameFB ? m_GameFB->getColorAttachmentRendererID() : 0;
    }

    void RenderSystem::onWindowResize(uint32_t width, uint32_t height)
    {
        if (!m_Initialized)
            return;
        ensureFramebufferSize(m_SceneFB, width, height);
        ensureFramebufferSize(m_GameFB, width, height);
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
            break;
        default:
            break;
        }
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
        uint32_t selected_entity_id = kInvalidEntityID;
        if (editor_ext)
        {
            use_game_camera = editor_ext->use_game_camera;
            if (HasFlag(flags, RenderFlags::SelectionOutline))
            {
                selected_entity_id = editor_ext->selected_entity_id;
            }
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
        pkt.selectedEntityID = editor_ext ? editor_ext->selected_entity_id : kInvalidEntityID;

        // B) write packet
        pkt.frame.viewProj = projM * viewM;
        pkt.frame.cameraPos = cameraPos;
        pkt.frame.time = frame_context.dt;

        if (cache_editor_camera_state)
        {
            m_LastView = viewM;
            m_LastProj = projM;
        }

        // C) collect lights
        pkt.lights.dir.intensity = 0.0f;
        pkt.lights.points.reserve(kMaxPointLights);
        if (scene)
        {
            auto &reg = scene->getRegistry();

            auto dirView = reg.view<Hybrid::TransformComponent, Hybrid::DirectionalLightComponent>();
            for (auto e : dirView)
            {
                const auto &tc = dirView.get<Hybrid::TransformComponent>(e);
                const auto &dl = dirView.get<Hybrid::DirectionalLightComponent>(e);
                if (!dl.Enabled)
                    continue;
                pkt.lights.dir.color = dl.Color;
                pkt.lights.dir.intensity = dl.Intensity;
                pkt.lights.dir.direction = lightDirectionFromTransform(tc);
                break;
            }

            auto ptView = reg.view<Hybrid::TransformComponent, Hybrid::PointLightComponent>();
            for (auto e : ptView)
            {
                if ((int)pkt.lights.points.size() >= kMaxPointLights)
                    break;
                const auto &tc = ptView.get<Hybrid::TransformComponent>(e);
                const auto &pl = ptView.get<Hybrid::PointLightComponent>(e);
                if (!pl.Enabled)
                    continue;

                RenderPointLightData p;
                p.color = pl.Color;
                p.intensity = pl.Intensity;
                p.position = tc.Position;
                p.range = pl.Range;
                pkt.lights.points.push_back(p);
            }
        }

        // D) collect draw items (pure ECS->packet)
        if (scene)
        {
            auto &registry = scene->getRegistry();
            auto renderView = registry.view<Hybrid::TransformComponent, Hybrid::MeshRendererComponent>();

            pkt.items.reserve(renderView.size_hint());
            for (auto e : renderView)
            {
                const auto &tr = renderView.get<Hybrid::TransformComponent>(e);
                const auto &mr = renderView.get<Hybrid::MeshRendererComponent>(e);
                if (!mr.Enabled)
                    continue;

                RenderDrawItem item;
                item.meshId = mr.Mesh;
                item.materialId = mr.Material;
                item.model = tr.WorldMatrix;
                item.tint = mr.Tint;
                item.entityID = (uint32_t)entt::to_integral(e);
                item.selected = (selected_entity_id != kInvalidEntityID && item.entityID == selected_entity_id);
                pkt.items.push_back(item);
            }
        }

        return pkt;
    }

    void RenderSystem::renderFrame(const FrameContext& frame_context,
                                   RenderFlags flags,
                                   const EditorRenderExt* editor_ext)
    {
        auto make_pipeline_callbacks =
            [this]()
        {
            RenderPipelineCallbacks callbacks;
            callbacks.forward = [this](RenderContext& context)
            {
                m_ForwardPass.execute(context);
            };
            callbacks.picking = [this](RenderContext& context)
            {
                m_PickingPass.execute(context);
            };
            callbacks.selection_outline = [this](RenderContext& context)
            {
                m_SelectionOutlinePass.execute(context);
            };
            callbacks.gizmos = [this](RenderContext& context)
            {
                m_GizmoPass.execute(context);
            };
            // callbacks.grid = [this](RenderContext& context)
            // {
            //     m_GridPass.execute(context);
            // };
            callbacks.shadows = [this](RenderContext& context)
            {
                m_ShadowPass.execute(context);
            };
            callbacks.post_process = [this](RenderContext& context)
            {
                m_PostProcessPass.execute(context);
            };
            // callbacks.debug_normals = [this](RenderContext& context)
            // {
            //     m_DebugNormalsPass.execute(context);
            // };
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
                ensureFramebufferSize(m_SceneFB,
                                      static_cast<uint32_t>(scene_frame.viewport_size.x),
                                      static_cast<uint32_t>(scene_frame.viewport_size.y));

                EditorRenderExt scene_ext = *editor_ext;
                scene_ext.use_game_camera = false;
                auto scene_packet = buildRenderPacket(scene_frame, flags, &scene_ext, true);
                RenderContext scene_context{};
                scene_context.frame = &scene_frame;
                scene_context.packet = &scene_packet;
                scene_context.flags = flags;
                scene_context.window_handle = window_handle;
                scene_context.framebuffer = m_SceneFB;
                scene_context.asset_manager = m_AssetManager;
                scene_context.shader_library = &m_ShaderLibrary;
                scene_context.material_system = &m_MaterialSystem;
                scene_context.resolve_mesh_gpu = [this](AssetID mesh_id, const std::shared_ptr<Mesh>& mesh) -> MeshGPU*
                {
                    return getOrCreateMeshGPU(mesh_id, mesh);
                };
                scene_context.mesh_shader = m_MeshShader;
                scene_context.box_colider_shader = m_DebugBoxShader;
                m_RenderPipeline.execute(scene_context, make_pipeline_callbacks());
                rendered_any = true;
            }

            if (editor_ext->render_game_view &&
                editor_ext->game_viewport_size.x > 0.0f &&
                editor_ext->game_viewport_size.y > 0.0f)
            {
                FrameContext game_frame = frame_context;
                game_frame.viewport_size = editor_ext->game_viewport_size;
                ensureFramebufferSize(m_GameFB,
                                      static_cast<uint32_t>(game_frame.viewport_size.x),
                                      static_cast<uint32_t>(game_frame.viewport_size.y));

                EditorRenderExt game_ext = *editor_ext;
                game_ext.use_game_camera = true;
                game_ext.has_editor_camera = false;
                const RenderFlags game_flags = RenderFlags::Forward;
                auto game_packet = buildRenderPacket(game_frame, game_flags, &game_ext, false);
                RenderContext game_context{};
                game_context.frame = &game_frame;
                game_context.packet = &game_packet;
                game_context.flags = game_flags;
                game_context.window_handle = window_handle;
                game_context.framebuffer = m_GameFB;
                game_context.asset_manager = m_AssetManager;
                game_context.shader_library = &m_ShaderLibrary;
                game_context.material_system = &m_MaterialSystem;
                game_context.resolve_mesh_gpu = [this](AssetID mesh_id, const std::shared_ptr<Mesh>& mesh) -> MeshGPU*
                {
                    return getOrCreateMeshGPU(mesh_id, mesh);
                };
                game_context.mesh_shader = m_MeshShader;
                game_context.box_colider_shader = m_DebugBoxShader;
                m_RenderPipeline.execute(game_context, make_pipeline_callbacks());
                rendered_any = true;
            }

            if (rendered_any)
                return;
        }

        if (frame_context.viewport_size.x <= 0.0f || frame_context.viewport_size.y <= 0.0f)
            return;

        ensureFramebufferSize(m_SceneFB,
                              static_cast<uint32_t>(frame_context.viewport_size.x),
                              static_cast<uint32_t>(frame_context.viewport_size.y));

        auto packet = buildRenderPacket(frame_context, flags, editor_ext, true);
        RenderContext context{};
        context.frame = &frame_context;
        context.packet = &packet;
        context.flags = flags;
        context.window_handle = window_handle;
        context.framebuffer = m_SceneFB;
        context.asset_manager = m_AssetManager;
        context.shader_library = &m_ShaderLibrary;
        context.material_system = &m_MaterialSystem;
        context.resolve_mesh_gpu = [this](AssetID mesh_id, const std::shared_ptr<Mesh>& mesh) -> MeshGPU*
        {
            return getOrCreateMeshGPU(mesh_id, mesh);
        };
        context.mesh_shader = m_MeshShader;
        context.box_colider_shader = m_DebugBoxShader;
        m_RenderPipeline.execute(context, make_pipeline_callbacks());
    }
    uint32_t RenderSystem::readEntityID(int x, int y) const
    {
        if (!m_SceneFB)
            return 0;
        if (x < 0 || y < 0)
            return 0;
        if (x >= static_cast<int>(m_SceneFB->getWidth()) ||
            y >= static_cast<int>(m_SceneFB->getHeight()))
            return 0;

        m_SceneFB->bind();

        // Ensure the picking pass writes to the second color attachment (entity ID buffer).Q
        glReadBuffer(GL_COLOR_ATTACHMENT1);

        uint32_t encoded_id = 0;
        glReadPixels(x, y, 1, 1, GL_RED_INTEGER, GL_UNSIGNED_INT, &encoded_id);

        m_SceneFB->unbind();
        return decodeEntityID(encoded_id);
    }

}
