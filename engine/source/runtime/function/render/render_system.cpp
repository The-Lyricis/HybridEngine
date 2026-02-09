#include "render_system.h"

#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <algorithm>

#include "runtime/function/render/renderer.h"
#include "runtime/function/render/render_command.h"
#include "runtime/function/render/vertex_array.h"
#include "runtime/function/render/shader.h"
#include "runtime/function/render/framebuffer.h"

namespace Hybrid {

    void RenderSystem::initialize(void* glfwWindowHandle) {
        if (m_Initialized) return;

        // Renderer 初始化（只一次）
        Renderer::Init();

        // 创建三角形资源
        createTriangleResources();

        // 初始化一个 framebuffer（先用窗口尺寸）
        GLFWwindow* window = static_cast<GLFWwindow*>(glfwWindowHandle);
        int w = 0, h = 0;
        glfwGetFramebufferSize(window, &w, &h);

        FramebufferSpec spec;
        spec.width = (uint32_t)std::max(1, w);
        spec.height = (uint32_t)std::max(1, h);
        m_SceneFB = std::make_shared<Framebuffer>(spec);

        m_Initialized = true;
    }

    void RenderSystem::createTriangleResources() {
        static float s_TriVertices[] = {
            // x,    y,    z,    r,   g,   b,   a
            -0.5f, -0.5f, 0.0f, 1.f, 0.f, 0.f, 1.f,
             0.5f, -0.5f, 0.0f, 0.f, 1.f, 0.f, 1.f,
             0.0f,  0.5f, 0.0f, 0.f, 0.f, 1.f, 1.f,
        };
        static uint32_t s_TriIndices[] = { 0, 1, 2 };

        m_TriangleVAO = std::make_shared<VertexArray>();
        auto vb = std::make_shared<VertexBuffer>(s_TriVertices, sizeof(s_TriVertices));
        auto ib = std::make_shared<IndexBuffer>(s_TriIndices, 3);

        m_TriangleVAO->SetVertexBuffer(vb);
        m_TriangleVAO->SetIndexBuffer(ib);

        const std::string vs = R"(
#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec4 aColor;
out vec4 vColor;
void main() {
    vColor = aColor;
    gl_Position = vec4(aPos, 1.0);
}
)";

        const std::string fs = R"(
#version 330 core
in vec4 vColor;
out vec4 FragColor;
void main() {
    FragColor = vColor;
}
)";

        m_TriangleShader = std::make_shared<Shader>(vs, fs);
    }

    void RenderSystem::ensureFramebufferSize(uint32_t w, uint32_t h) {
        w = std::max(1u, w);
        h = std::max(1u, h);
        if (!m_SceneFB) {
            FramebufferSpec spec{ w, h };
            m_SceneFB = std::make_shared<Framebuffer>(spec);
            return;
        }
        m_SceneFB->Resize(w, h);
    }

    uint32_t RenderSystem::getSceneColorTexture() const {
        return m_SceneFB ? m_SceneFB->GetColorAttachmentRendererID() : 0;
    }

    void RenderSystem::renderFrame(const glm::vec2& viewportSize, void* glfwWindowHandle) {
        if (!m_Initialized) initialize(glfwWindowHandle);

        // 1) 确保 FBO 尺寸匹配 Viewport
        ensureFramebufferSize((uint32_t)viewportSize.x, (uint32_t)viewportSize.y);

        // 2) 渲染到 FBO
        m_SceneFB->Bind();
        RenderCommand::SetViewport(0, 0, m_SceneFB->GetWidth(), m_SceneFB->GetHeight());

        Renderer::BeginFrame({ 0.1f, 0.1f, 0.12f, 1.0f });
        Renderer::Submit(m_TriangleVAO, m_TriangleShader);
        Renderer::EndFrame();

        m_SceneFB->Unbind();

        // 3) 清屏默认帧缓冲（防拖影）
        GLFWwindow* window = static_cast<GLFWwindow*>(glfwWindowHandle);
        int display_w = 0, display_h = 0;
        glfwGetFramebufferSize(window, &display_w, &display_h);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        RenderCommand::SetViewport(0, 0, display_w, display_h);
        RenderCommand::SetClearColor({ 0.08f, 0.08f, 0.09f, 1.0f });
        RenderCommand::Clear();
    }

} // namespace Hybrid
