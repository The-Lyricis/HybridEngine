#pragma once

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "runtime/modules/asset/asset_manager.h"
#include "runtime/modules/asset/cubemap_image.h"
#include "runtime/modules/asset/material.h"
#include "runtime/modules/asset/mesh.h"
#include "runtime/modules/render/runtime/editor_render_ext.h"
#include "runtime/modules/render/runtime/passes/scene_pass.h"
#include "runtime/modules/render/runtime/passes/gizmo_pass.h"
#include "runtime/modules/render/runtime/passes/picking_pass.h"
#include "runtime/modules/render/runtime/passes/post_process_pass.h"
#include "runtime/modules/render/runtime/passes/skybox_pass.h"
#include "runtime/modules/render/runtime/render_context.h"
#include "runtime/modules/render/runtime/frame_context.h"
#include "runtime/modules/render/runtime/material_system.h"
#include "runtime/modules/render/runtime/mesh_gpu.h"
#include "runtime/modules/render/runtime/render_uniforms.h"
#include "runtime/modules/render/runtime/selection_overlay_style.h"
#include "runtime/modules/render/runtime/passes/selection_overlay_pass.h"
#include "runtime/modules/render/runtime/passes/selection_mask_pass.h"
#include "runtime/modules/render/runtime/render_packet.h"
#include "runtime/modules/render/runtime/render_pipeline.h"
#include "runtime/modules/render/runtime/render_flags.h"
#include "runtime/modules/render/runtime/shader_library.h"
#include "runtime/modules/render/runtime/passes/shadow_pass.h"
#include "runtime/modules/render/public/framebuffer.h"
#include "runtime/modules/render/public/texture.h"
#include "runtime/modules/render/public/texture_uploader.h"
#include "runtime/core/base/intersection.h"

namespace Hybrid
{
    struct RenderStats
    {
        float frame_time_ms = 0.0f;
        float fps = 0.0f;
        float render_cpu_time_ms = 0.0f;
        uint32_t scene_renderers = 0;
        uint32_t scene_submeshes = 0;
        uint32_t submitted_draw_calls = 0;
        uint32_t submitted_triangles = 0;
        uint32_t submitted_entities = 0;
        uint32_t submitted_opaque_items = 0;
        uint32_t submitted_transparent_items = 0;
        uint32_t point_lights = 0;
        uint32_t tested_items = 0;
        uint32_t culled_items = 0;
    };

    class Framebuffer;
    class VertexArray;
    class VertexBuffer;
    class IndexBuffer;
    class UniformBuffer;
    class Shader;
    class Scene;

    // Owns runtime render resources and executes render passes from FrameContext/Flags.
    class RenderSystem
    {
    public:
        RenderSystem() = default;
        ~RenderSystem() = default;

        void initialize(void* glfwWindowHandle);
        void update(float dt);
        void setAssetManager(std::shared_ptr<AssetManager> mgr);
        void setScene(std::shared_ptr<Scene> scene) { m_Scene = std::move(scene); }

        uint32_t getSceneColorTexture() const;
        uint32_t getGameColorTexture() const;
        void onWindowResize(uint32_t width, uint32_t height);
        uint32_t readEntityID(int x, int y) const;
        void invalidateAsset(AssetID id, AssetType type);

        // Per-frame render entry driven by context + feature flags.
        void renderFrame(const FrameContext& frame_context,
                         RenderFlags flags,
                         const EditorRenderExt* editor_ext = nullptr);
        const glm::mat4& getLastView() const { return m_LastView; }
        const glm::mat4& getLastProj() const { return m_LastProj; }
        const RenderStats& getStats() const { return m_Stats; }

    private:
        void ensureFramebuffer(std::shared_ptr<Framebuffer>& framebuffer, const FramebufferSpec& spec);
        void ensureSceneViewRenderTargets(uint32_t w, uint32_t h);
        bool loadBuiltinShaders();
        void ensureGlobalUniformBuffers();
        void configureShaderBindings();
        void updateFrameUBO(const RenderPacket& packet, const glm::vec2& viewport_size);
        void updateLightUBO(const RenderPacket& packet);
        void collectPacketLights(RenderPacket& packet) const;
        void collectPacketDrawItems(RenderPacket& packet, const Frustum& frustum);
        void sortRenderPacket(RenderPacket& packet) const;
        void updateStatsFromPacket(const RenderPacket& packet, float render_cpu_time_ms);
        TexturePtr getOrCreateCubemapTexture(AssetID id);
        TexturePtr getDefaultCubemapTexture();

        // Extract ECS data + camera/light state into a draw packet.
        RenderPacket buildRenderPacket(const FrameContext& frame_context,
                                       RenderFlags flags,
                                       const EditorRenderExt* editor_ext,
                                       bool cache_editor_camera_state = true);
        MeshGPU* getOrCreateMeshGPU(AssetID id, const std::shared_ptr<Mesh>& mesh);

    private:
        std::shared_ptr<Scene> m_Scene; // Fallback scene source when frame context has no scene.

        std::shared_ptr<Framebuffer> m_SceneFB;
        std::shared_ptr<Framebuffer> m_SelectionFB;
        std::shared_ptr<Framebuffer> m_GameFB;
        std::shared_ptr<UniformBuffer> m_FrameUBO;
        std::shared_ptr<UniformBuffer> m_LightUBO;
        SelectionOverlayStyle m_SelectionOverlayStyle;
        std::shared_ptr<Shader> m_SceneShader;
        std::shared_ptr<Shader> m_SkyboxShader;
        std::shared_ptr<Shader> m_ColliderDebugShader;
        ShaderLibrary m_ShaderLibrary;
        MaterialSystem m_MaterialSystem;
        RenderPipeline m_RenderPipeline;
        ScenePass m_ScenePass;
        SkyboxPass m_SkyboxPass;
        PickingPass m_PickingPass;
        GizmoPass m_GizmoPass;
        ShadowPass m_ShadowPass;
        PostProcessPass m_PostProcessPass;
        SelectionMaskPass m_SelectionMaskPass;
        SelectionOverlayPass m_SelectionOverlayPass;

        std::shared_ptr<AssetManager> m_AssetManager;
        std::unordered_map<AssetID, MeshGPU, AssetID::Hasher> m_MeshCache;
        std::unordered_map<AssetID, TexturePtr, AssetID::Hasher> m_CubemapCache;
        TexturePtr m_DefaultCubemapTexture;
        std::unique_ptr<TextureUploader> m_TextureUploader;

        bool m_Initialized = false; // Render backend init state.
        float m_ShaderReloadTimer = 0.0f;
        float m_ShaderReloadInterval = 0.5f;
        RenderStats m_Stats{};

        glm::mat4 m_LastView = glm::mat4(1.0f);
        glm::mat4 m_LastProj = glm::mat4(1.0f);
    };
} // namespace Hybrid
