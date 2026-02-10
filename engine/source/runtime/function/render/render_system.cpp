#include "render_system.h"

#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include <algorithm>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "runtime/function/render/renderer.h"
#include "runtime/function/render/render_command.h"
#include "runtime/function/render/vertex_array.h"
#include "runtime/function/render/shader.h"
#include "runtime/function/render/framebuffer.h"

namespace Hybrid {

    void RenderSystem::initialize(void* glfwWindowHandle) {
        if (m_Initialized) return;

        Renderer::initialize();
        createCubeResources();

        GLFWwindow* window = static_cast<GLFWwindow*>(glfwWindowHandle);
        int w = 0, h = 0;
        glfwGetFramebufferSize(window, &w, &h);

        FramebufferSpec spec;
        spec.width = (uint32_t)std::max(1, w);
        spec.height = (uint32_t)std::max(1, h);
        m_SceneFB = std::make_shared<Framebuffer>(spec);

        // 相机初始投影（后续每帧会随 viewport 变化）
        m_Camera.setViewportSize((float)spec.width, (float)spec.height);

        m_Initialized = true;
    }

    void RenderSystem::createCubeResources() {
        static float s_CubeVertices[] = {
            // x,    y,    z,    r,   g,   b,   a
            -0.5f, -0.5f, -0.5f, 1.f, 0.f, 0.f, 1.f, // 0
             0.5f, -0.5f, -0.5f, 0.f, 1.f, 0.f, 1.f, // 1
             0.5f,  0.5f, -0.5f, 0.f, 0.f, 1.f, 1.f, // 2
            -0.5f,  0.5f, -0.5f, 1.f, 1.f, 0.f, 1.f, // 3
            -0.5f, -0.5f,  0.5f, 1.f, 0.f, 1.f, 1.f, // 4
             0.5f, -0.5f,  0.5f, 0.f, 1.f, 1.f, 1.f, // 5
             0.5f,  0.5f,  0.5f, 1.f, 1.f, 1.f, 1.f, // 6
            -0.5f,  0.5f,  0.5f, 0.2f, 0.2f, 0.2f, 1.f  // 7
        };

        static uint32_t s_CubeIndices[] = {
            // back (-Z)
            0, 1, 2,  2, 3, 0,
            // front (+Z)
            4, 5, 6,  6, 7, 4,
            // left (-X)
            0, 3, 7,  7, 4, 0,
            // right (+X)
            1, 5, 6,  6, 2, 1,
            // bottom (-Y)
            0, 1, 5,  5, 4, 0,
            // top (+Y)
            3, 2, 6,  6, 7, 3
        };

        m_CubeVAO = std::make_shared<VertexArray>();
        auto vb = std::make_shared<VertexBuffer>(s_CubeVertices, sizeof(s_CubeVertices));
        auto ib = std::make_shared<IndexBuffer>(
            s_CubeIndices,
            (uint32_t)(sizeof(s_CubeIndices) / sizeof(uint32_t))
        );

        m_CubeVAO->setVertexBuffer(vb);
        m_CubeVAO->setIndexBuffer(ib);

        const std::string vs = R"(
#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec4 aColor;

uniform mat4 u_ViewProjection;
uniform mat4 u_Model;

out vec4 vColor;

void main() {
    vColor = aColor;
    gl_Position = u_ViewProjection * u_Model * vec4(aPos, 1.0);
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

        m_CubeShader = std::make_shared<Shader>(vs, fs);
    }

    void RenderSystem::ensureFramebufferSize(uint32_t w, uint32_t h) {
        w = std::max(1u, w);
        h = std::max(1u, h);

        if (!m_SceneFB) {
            FramebufferSpec spec{ w, h };
            m_SceneFB = std::make_shared<Framebuffer>(spec);
        }
        else {
            m_SceneFB->resize(w, h);
        }

        // 相机投影随 viewport 变化
        m_Camera.setViewportSize((float)w, (float)h);
    }

    uint32_t RenderSystem::getSceneColorTexture() const {
        return m_SceneFB ? m_SceneFB->getColorAttachmentRendererID() : 0;
    }

    void RenderSystem::renderFrame(const glm::vec2& viewportSize,
        void* glfwWindowHandle,
        float dt,
        bool viewportActive,
        const InputState& input) {
        if (!m_Initialized) initialize(glfwWindowHandle);

        // 1) FBO 尺寸匹配 viewport
        ensureFramebufferSize((uint32_t)viewportSize.x, (uint32_t)viewportSize.y);

        // 2) 解析输入（GLFW key/button 常量）
        //    注意：只有 viewportActive 时才允许控制相机（避免 UI 输入冲突）
        const bool rmbDown = viewportActive && input.isMouseDown(GLFW_MOUSE_BUTTON_RIGHT);

        const float mdx = viewportActive ? input.getMouseDeltaX() : 0.0f;
        const float mdy = viewportActive ? input.getMouseDeltaY() : 0.0f;

        // 你可按喜好决定：滚轮是否只在 viewportActive 时生效
        const float scrollY = viewportActive ? input.getScrollDeltaY() : 0.0f;

        const bool keyW = viewportActive && input.isKeyDown(GLFW_KEY_W);
        const bool keyA = viewportActive && input.isKeyDown(GLFW_KEY_A);
        const bool keyS = viewportActive && input.isKeyDown(GLFW_KEY_S);
        const bool keyD = viewportActive && input.isKeyDown(GLFW_KEY_D);
        const bool keyQ = viewportActive && input.isKeyDown(GLFW_KEY_Q);
        const bool keyE = viewportActive && input.isKeyDown(GLFW_KEY_E);

        // 3) 更新相机（你的独立相机类）
        m_Camera.update(dt, viewportActive, rmbDown, mdx, mdy,
            keyW, keyA, keyS, keyD, keyQ, keyE,
            scrollY);

        // 4) 渲染到 FBO
        m_SceneFB->bind();
        RenderCommand::setViewport(0, 0, m_SceneFB->getWidth(), m_SceneFB->getHeight());

        Renderer::beginFrame({ 0.1f, 0.1f, 0.12f, 1.0f });

        // Model：旋转立方体（可视化验证 MVP）
        const float t = (float)glfwGetTime();
        glm::mat4 model(1.0f);
        model = glm::rotate(model, t * 0.8f, glm::vec3(0, 1, 0));
        model = glm::rotate(model, t * 0.35f, glm::vec3(1, 0, 0));

        // 设置 uniform（要求 Shader::SetMat4 存在）
        m_CubeShader->bind();
        m_CubeShader->setMat4("u_ViewProjection", m_Camera.getViewProj());
        m_CubeShader->setMat4("u_Model", model);

        Renderer::submit(m_CubeVAO, m_CubeShader);
        Renderer::endFrame();

        m_SceneFB->unbind();

        // 5) 清屏默认帧缓冲（防 UI 拖影）
        GLFWwindow* window = static_cast<GLFWwindow*>(glfwWindowHandle);
        int display_w = 0, display_h = 0;
        glfwGetFramebufferSize(window, &display_w, &display_h);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        RenderCommand::setViewport(0, 0, display_w, display_h);
        RenderCommand::setClearColor({ 0.08f, 0.08f, 0.09f, 1.0f });
        RenderCommand::clear();
    }

} // namespace Hybrid
