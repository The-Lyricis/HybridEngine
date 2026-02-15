#pragma once
#include <memory>
#include <cstdint>
#include <glm/vec2.hpp>
#include <unordered_map>

#include "runtime/function/input/input_state.h"
#include "runtime/function/render/editor_camera.h" // 你自己的相机类头文件路径
#include "runtime/function/asset/asset_manager.h"
#include "runtime/function/asset/mesh.h"
#include "runtime/function/asset/material.h"
#include "runtime/function/scene/components.h"
#include "runtime/function/render/texture.h"

namespace Hybrid {

    class Framebuffer;
    class VertexArray;
    class VertexBuffer;
    class IndexBuffer;
    class Shader;
    class Scene;

    // RenderSystem: owns scene resources and issues per-frame rendering.
    class RenderSystem {
    public:
        RenderSystem() = default;
        ~RenderSystem() = default;

        void initialize(void* glfwWindowHandle);
        void setAssetManager(std::shared_ptr<AssetManager> mgr) { m_AssetManager = std::move(mgr); }

        void setScene(std::shared_ptr<Scene> scene) { m_Scene = std::move(scene); }

        uint32_t getSceneColorTexture() const;

        void onWindowResize(uint32_t width, uint32_t height);

        //输入收敛为 InputState
        void renderFrame(const glm::vec2& viewportSize,
            void* glfwWindowHandle,
            float dt,
            bool viewportActive,
            const InputState& input,
            bool useGameCamera);

    private:
        void createCubeResources();
        void ensureFramebufferSize(uint32_t w, uint32_t h);
        void createMeshShader();

        struct MeshGPU {
            std::shared_ptr<VertexArray> vao;
            std::shared_ptr<VertexBuffer> vb;
            std::shared_ptr<IndexBuffer> ib;
            std::vector<Submesh> submeshes;
        };
        struct MaterialGPU {
            MaterialData data;
            TexturePtr albedo, normal, mr, ao, emissive;
            void bind(Shader& shader) const;
        };

        MeshGPU*     getOrCreateMeshGPU(AssetID id, const std::shared_ptr<Mesh>& mesh);
        MaterialGPU* getOrCreateMaterialGPU(AssetID id, const std::shared_ptr<Material>& mat);
        MeshGPU*     getDefaultMeshGPU();
        MaterialGPU* getDefaultMaterialGPU();

    private:
        std::shared_ptr<Scene> m_Scene;

        std::shared_ptr<Framebuffer> m_SceneFB;
        std::shared_ptr<VertexArray> m_CubeVAO;
        std::shared_ptr<Shader>      m_CubeShader;
        std::shared_ptr<Shader>      m_MeshShader;

        EditorCamera m_Camera;

        std::shared_ptr<AssetManager> m_AssetManager;
        std::unordered_map<AssetID, MeshGPU, AssetID::Hasher> m_MeshCache;
        std::unordered_map<AssetID, MaterialGPU, AssetID::Hasher> m_MatCache;

        bool m_Initialized = false;
    };

} // namespace Hybrid
