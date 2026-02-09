#pragma once
#include <memory>
#include <cstdint>
#include <glm/vec2.hpp>

namespace Hybrid {

    class Framebuffer;
    class VertexArray;
    class Shader;

    class RenderSystem {
    public:
        RenderSystem() = default;
        ~RenderSystem() = default;

        // OpenGL 已初始化（gladLoadGL 成功）后调用
        void initialize(void* glfwWindowHandle);

        // 每帧调用：根据 viewport size 渲染到 FBO，并准备好给 UI 展示的纹理
        void renderFrame(const glm::vec2& viewportSize, void* glfwWindowHandle);

        // 提供给 EditorUI::drawViewport 使用
        uint32_t getSceneColorTexture() const;

    private:
        void createTriangleResources();
        void ensureFramebufferSize(uint32_t w, uint32_t h);

    private:
        std::shared_ptr<Framebuffer> m_SceneFB;
        std::shared_ptr<VertexArray> m_TriangleVAO;
        std::shared_ptr<Shader> m_TriangleShader;

        bool m_Initialized = false;
    };

} // namespace Hybrid
