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
#include "runtime/modules/asset/material.h"
#include "runtime/modules/asset/mesh.h"
#include "runtime/modules/render/runtime/editor_render_ext.h"
#include "runtime/modules/render/runtime/frame_context.h"
#include "runtime/modules/render/runtime/render_flags.h"
#include "runtime/modules/render/public/texture.h"

namespace Hybrid
{

    class Framebuffer;
    class VertexArray;
    class VertexBuffer;
    class IndexBuffer;
    class Shader;
    class Scene;

    // Owns runtime render resources and executes render passes from FrameContext/Flags.
    class RenderSystem
    {
    public:
        RenderSystem() = default;
        ~RenderSystem() = default;

        void initialize(void *glfwWindowHandle);
        void setAssetManager(std::shared_ptr<AssetManager> mgr) { m_AssetManager = std::move(mgr); }
        void setScene(std::shared_ptr<Scene> scene) { m_Scene = std::move(scene); }

        uint32_t getSceneColorTexture() const;
        void onWindowResize(uint32_t width, uint32_t height);
        uint32_t readEntityID(int x, int y) const;

        // Per-frame render entry driven by context + feature flags.
        void renderFrame(const FrameContext& frame_context,
                         RenderFlags flags,
                         const EditorRenderExt* editor_ext = nullptr);
        const glm::mat4& getLastView() const { return m_LastView; }
        const glm::mat4& getLastProj() const { return m_LastProj; }

    private:
        void createCubeResources();
        void ensureFramebufferSize(uint32_t w, uint32_t h);
        void createMeshShader();
        void createDebugBoxShader();

        struct MeshGPU
        {
            std::shared_ptr<VertexArray> vao;
            std::shared_ptr<VertexBuffer> vb;
            std::shared_ptr<IndexBuffer> ib;
            std::vector<Submesh> submeshes;
        };

        struct MaterialGPU
        {
            MaterialData data;
            TexturePtr albedo;
            TexturePtr normal;
            TexturePtr mr;
            TexturePtr ao;
            TexturePtr emissive;
            void bind(Shader &shader) const;
        };

        struct FrameData
        {
            glm::mat4 viewProj{1.0f};
            glm::vec3 cameraPos{0.0f};
            float time = 0.0f;
        };

        struct DirLightData
        {
            glm::vec3 color{1.0f};
            float intensity = 0.0f;
            glm::vec3 direction{0.0f, -1.0f, 0.0f};
            float pad = 0.0f;
        };

        struct PointLightData
        {
            glm::vec3 color{1.0f};
            float intensity = 0.0f;
            glm::vec3 position{0.0f};
            float range = 1.0f;
        };

        static constexpr int kMaxPointLights = 16;

        struct LightData
        {
            DirLightData dir;
            std::vector<PointLightData> points;
        };

        struct DrawItem
        {
            AssetID meshId{};
            AssetID materialId{};
            int primitive = 0;
            glm::mat4 model{1.0f};
            glm::vec4 tint{1.0f};
            uint32_t entityID = 0;
            bool selected = false;
        };

        
        struct RenderPacket
        {
            FrameData frame;
            LightData lights;
            std::vector<DrawItem> items;
            bool showColliderDebug = false;
            std::shared_ptr<Scene> scene;
        };

        // Extract ECS data + camera/light state into a draw packet.
        RenderPacket buildRenderPacket(const FrameContext& frame_context,
                                       RenderFlags flags,
                                       const EditorRenderExt* editor_ext);
        // Dispatch pass execution by RenderFlags.
        void executePasses(const RenderPacket& packet, RenderFlags flags, void* glfwWindowHandle);
        // Execute the main forward pass and resolve color/id targets.
        void executeForwardPass(const RenderPacket &packet, void *glfwWindowHandle);
        void executePickingPass(const RenderPacket& packet, void* glfwWindowHandle);
        void executeSelectionOutlinePass(const RenderPacket& packet, void* glfwWindowHandle);
        void executeGizmoPass(const RenderPacket& packet, void* glfwWindowHandle);
        void executeGridPass(const RenderPacket& packet, void* glfwWindowHandle);
        void executeShadowPass(const RenderPacket& packet, void* glfwWindowHandle);
        void executePostProcessPass(const RenderPacket& packet, void* glfwWindowHandle);
        void executeDebugNormalsPass(const RenderPacket& packet, void* glfwWindowHandle);

        MeshGPU *getOrCreateMeshGPU(AssetID id, const std::shared_ptr<Mesh> &mesh);
        MaterialGPU *getOrCreateMaterialGPU(AssetID id, const std::shared_ptr<Material> &mat);
        TexturePtr createSolidTexture(uint8_t r, uint8_t g, uint8_t b, uint8_t a);
        

    private:
        std::shared_ptr<Scene> m_Scene; // Fallback scene source when frame context has no scene.

        std::shared_ptr<Framebuffer> m_SceneFB;
        std::shared_ptr<VertexArray> m_CubeVAO;
        std::shared_ptr<Shader> m_CubeShader;
        std::shared_ptr<Shader> m_MeshShader;
        std::shared_ptr<Shader> m_DebugBoxShader;

        std::shared_ptr<AssetManager> m_AssetManager;
        std::unordered_map<AssetID, MeshGPU, AssetID::Hasher> m_MeshCache;
        std::unordered_map<AssetID, MaterialGPU, AssetID::Hasher> m_MatCache;

        TexturePtr m_DefaultAlbedoTex;
        TexturePtr m_DefaultNormalTex;
        TexturePtr m_DefaultMRTex;
        TexturePtr m_DefaultAOTex;
        TexturePtr m_DefaultEmissiveTex;

        bool m_Initialized = false; // Render backend init state.

        glm::mat4 m_LastView = glm::mat4(1.0f);
        glm::mat4 m_LastProj = glm::mat4(1.0f);

        
    };

} // namespace Hybrid


