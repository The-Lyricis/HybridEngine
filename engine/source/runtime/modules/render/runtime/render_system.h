#pragma once

#include <cstdint>
#include <array>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "runtime/modules/asset/asset_type.h"
#include "runtime/modules/render/runtime/passes/scene_pass.h"
#include "runtime/modules/render/runtime/builders/frame_view_resolver.h"
#include "runtime/modules/render/runtime/builders/render_packet_builder.h"
#include "runtime/modules/render/runtime/builders/render_context_builder.h"
#include "runtime/modules/render/runtime/builders/shadow_frame_builder.h"
#include "runtime/modules/render/runtime/passes/gizmo_pass.h"
#include "runtime/modules/render/runtime/passes/grid_pass.h"
#include "runtime/modules/render/runtime/passes/picking_pass.h"
#include "runtime/modules/render/runtime/passes/post_process_pass.h"
#include "runtime/modules/render/runtime/passes/skybox_pass.h"
#include "runtime/modules/render/runtime/frame_context.h"
#include "runtime/modules/render/runtime/material_system.h"
#include "runtime/modules/render/runtime/mesh_gpu.h"
#include "runtime/modules/render/runtime/passes/overlay_gizmo_pass.h"
#include "runtime/modules/render/runtime/render_uniforms.h"
#include "runtime/modules/render/runtime/render_shadow_settings.h"
#include "runtime/modules/render/runtime/selection_overlay_style.h"
#include "runtime/modules/render/runtime/passes/selection_overlay_pass.h"
#include "runtime/modules/render/runtime/passes/selection_mask_pass.h"
#include "runtime/modules/render/runtime/render_packet.h"
#include "runtime/modules/render/runtime/pipeline/render_pipeline.h"
#include "runtime/modules/render/runtime/pipeline/render_graph_resources.h"
#include "runtime/modules/render/runtime/render_flags.h"
#include "runtime/modules/render/runtime/render_frame_request.h"
#include "runtime/modules/render/runtime/shader_library.h"
#include "runtime/modules/render/runtime/passes/shadow_pass.h"
#include "runtime/modules/render/public/framebuffer.h"
#include "runtime/modules/render/public/texture.h"
#include "runtime/modules/render/public/texture_uploader.h"
#include "runtime/modules/render/rhi/render_device.h"

namespace Hybrid
{
    struct FrameContext;
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
        uint32_t shadow_caster_items = 0;
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
    class AssetManager;
    class Mesh;

    // Owns runtime render resources and executes render passes from FrameContext/Flags.
    class RenderSystem
    {
    public:
        RenderSystem(std::unique_ptr<IRenderDevice> device,
                     std::unique_ptr<ISwapchain> swapchain);
        ~RenderSystem();

        void initialize(uint32_t framebuffer_width, uint32_t framebuffer_height);
        void shutdown();
        bool isInitialized() const { return m_Initialized; }
        void update(float dt);
        void setAssetManager(std::shared_ptr<AssetManager> mgr);
        void setScene(std::shared_ptr<Scene> scene) { m_Scene = std::move(scene); }

        void onWindowResize(uint32_t width, uint32_t height);
        RhiStatus present();
        void invalidateAsset(AssetID id, AssetType type);
        // Rendering extensions must use graph resources and RHI handles; native
        // backend objects are intentionally not part of this contract.
        bool setRenderPipeline(std::unique_ptr<IRenderPipeline> pipeline);
        bool registerRenderFeature(std::shared_ptr<IRenderFeature> feature);
        bool unregisterRenderFeature(const std::string& name);
        const IRenderPipeline* renderPipeline() const { return m_RenderPipeline.get(); }

        // Per-frame multi-view render entry.
        RenderFrameResult renderFrame(const RenderFrameRequest& request);
        const glm::mat4& getLastView() const { return m_LastView; }
        const glm::mat4& getLastProj() const { return m_LastProj; }
        const RenderStats& getStats() const { return m_Stats; }
        IRenderDevice& device() const { return *m_RenderDevice; }
        ISwapchain* swapchain() const { return m_Swapchain.get(); }

    private:
        void ensureFramebuffer(std::shared_ptr<Framebuffer>& framebuffer, const FramebufferSpec& spec);
        struct ViewRenderTargets
        {
            std::shared_ptr<Framebuffer> main;
            std::shared_ptr<Framebuffer> selection;
            RenderGraphResourceRegistry graph_resources;
            uint64_t last_used_frame = 0;
        };
        ViewRenderTargets& acquireViewTargets(RenderViewId id, const RenderViewRequest& view);
        bool loadBuiltinShaders();
        void ensureGlobalUniformBuffers();
        void configureShaderBindings();
        void updateFrameUBO(const RenderPacket& packet, const glm::vec2& viewport_size);
        void updateLightUBO(const RenderPacket& packet);
        void updateShadowUBO(const RenderPacket& packet);
        void updateStatsFromPacket(const RenderPacket& packet, float render_cpu_time_ms);
        TexturePtr getOrCreateCubemapTexture(AssetID id);
        TexturePtr getDefaultCubemapTexture();

        // Extract ECS data + camera/light state into a draw packet.
        RenderPacket buildRenderPacket(const FrameContext& frame_context,
                                       RenderFlags flags,
                                       const RenderViewRequest* view_request,
                                       bool cache_editor_camera_state = true);
        MeshGPU* getOrCreateMeshGPU(AssetID id, const std::shared_ptr<Mesh>& mesh);
        void destroyMeshGPU(MeshGPU& mesh_gpu);
        void clearMeshCache();
        void renderFrameInternal(const FrameContext& frame_context,
                                 const RenderViewRequest& view,
                                 const ResolvedRenderTargets& targets);

    private:
        std::unique_ptr<IRenderDevice> m_RenderDevice;
        std::unique_ptr<ISwapchain> m_Swapchain;
        std::shared_ptr<Scene> m_Scene; // Fallback scene source when frame context has no scene.

        std::unordered_map<RenderViewId, ViewRenderTargets> m_ViewTargets;
        uint64_t m_RenderFrameIndex = 0;
        std::array<std::shared_ptr<Framebuffer>, kMaxDirectionalShadowCascades> m_ShadowCascadeFBs{};
        std::shared_ptr<UniformBuffer> m_FrameUBO;
        std::shared_ptr<UniformBuffer> m_LightUBO;
        // Transitional shared data: legacy passes retain m_FrameUBO while RHI
        // passes consume this handle. It is removed with the final legacy pass.
        BufferHandle m_RhiFrameUniformBuffer;
        BufferHandle m_RhiLightUniformBuffer;
        BufferHandle m_RhiShadowUniformBuffer;
        SelectionOverlayStyle m_SelectionOverlayStyle;
        std::shared_ptr<Shader> m_SceneShader;
        std::shared_ptr<Shader> m_SkyboxShader;
        std::shared_ptr<Shader> m_ShadowShader;
        std::shared_ptr<Shader> m_ColliderDebugShader;
        ShaderLibrary m_ShaderLibrary;
        MaterialSystem m_MaterialSystem;
        std::unique_ptr<IRenderPipeline> m_RenderPipeline;
        ScenePass m_ScenePass;
        SkyboxPass m_SkyboxPass;
        PickingPass m_PickingPass;
        GizmoPass m_GizmoPass;
        OverlayGizmoPass m_OverlayGizmoPass;
        GridPass m_GridPass;
        ShadowPass m_ShadowPass;
        PostProcessPass m_PostProcessPass;
        SelectionMaskPass m_SelectionMaskPass;
        SelectionOverlayPass m_SelectionOverlayPass;

        std::shared_ptr<AssetManager> m_AssetManager;
        std::unordered_map<AssetID, MeshGPU, AssetID::Hasher> m_MeshCache;
        std::unordered_map<AssetID, TexturePtr, AssetID::Hasher> m_CubemapCache;
        TexturePtr m_DefaultCubemapTexture;
        std::unique_ptr<TextureUploader> m_TextureUploader;
        DirectionalShadowSettings m_DirectionalShadowSettings{};
        FrameViewResolver m_FrameViewResolver;
        ShadowFrameBuilder m_ShadowFrameBuilder;
        RenderPacketBuilder m_RenderPacketBuilder;
        RenderContextBuilder m_RenderContextBuilder;

        bool m_Initialized = false; // Render backend init state.
        float m_ShaderReloadTimer = 0.0f;
        float m_ShaderReloadInterval = 0.5f;
        RenderStats m_Stats{};

        glm::mat4 m_LastView = glm::mat4(1.0f);
        glm::mat4 m_LastProj = glm::mat4(1.0f);
    };
} // namespace Hybrid
