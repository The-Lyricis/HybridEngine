#include "render_system.h"

#include <GLFW/glfw3.h>

#include <algorithm>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/euler_angles.hpp>

#include "runtime/function/render/renderer.h"
#include "runtime/function/render/render_command.h"
#include "runtime/function/render/vertex_array.h"
#include "runtime/function/render/shader.h"
#include "runtime/function/render/framebuffer.h"
#include "runtime/function/scene/scene.h"
#include "runtime/function/scene/components.h"



namespace Hybrid {

    namespace
    {
        // TransformComponent -> Model Matrix
        static glm::mat4 BuildModel(const Hybrid::TransformComponent& tr)
        {
            glm::mat4 T = glm::translate(glm::mat4(1.0f), tr.Position);
            glm::mat4 R = glm::yawPitchRoll(tr.Rotation.y, tr.Rotation.x, tr.Rotation.z); // yaw, pitch, roll
            glm::mat4 S = glm::scale(glm::mat4(1.0f), tr.Scale);
            return T * R * S;
        }

        // Scene Camera (Transform + CameraComponent) -> ViewProjection
        static bool TryGetSceneViewProj(Hybrid::Scene& scene, float aspect, glm::mat4& outViewProj)
        {
            auto& reg = scene.GetRegistry();
            auto view = reg.view<Hybrid::TransformComponent, Hybrid::CameraComponent>();

            entt::entity mainCam = entt::null;
            for (auto e : view)
            {
                auto& cam = view.get<Hybrid::CameraComponent>(e);
                if (cam.Primary)
                {
                    mainCam = e;
                    break;
                }
            }
            if (mainCam == entt::null) return false;

            const auto& tr = reg.get<Hybrid::TransformComponent>(mainCam);
            const auto& cam = reg.get<Hybrid::CameraComponent>(mainCam);

            // Projection
            glm::mat4 proj = glm::perspective(glm::radians(cam.FovY), aspect, cam.Near, cam.Far);

            // View: inverse of camera world transform
            glm::mat4 T = glm::translate(glm::mat4(1.0f), tr.Position);
            glm::mat4 R = glm::yawPitchRoll(tr.Rotation.y, tr.Rotation.x, tr.Rotation.z);
            glm::mat4 viewMat = glm::inverse(T * R);

            outViewProj = proj * viewMat;
            return true;
        }
    }

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
        m_SceneFB = Framebuffer::Create(spec);

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

        m_CubeVAO = VertexArray::Create();
        auto vb = VertexBuffer::Create(
            s_CubeVertices,
            static_cast<uint32_t>(sizeof(s_CubeVertices))
        );
        auto ib = IndexBuffer::Create(
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

        m_CubeShader = Shader::Create(vs, fs);
    }

    void RenderSystem::ensureFramebufferSize(uint32_t w, uint32_t h) {
        w = std::max(1u, w);
        h = std::max(1u, h);

        if (!m_SceneFB) {
            FramebufferSpec spec{ w, h };
            m_SceneFB = Framebuffer::Create(spec);
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
        const InputState& input,
        bool useGameCamera)
    {
        if (!m_Initialized) initialize(glfwWindowHandle);

        // 1) FBO 尺寸匹配 viewport
        ensureFramebufferSize((uint32_t)viewportSize.x, (uint32_t)viewportSize.y);

        // 2) 解析输入（只在 Editor 预览且 viewportActive 时更新 EditorCamera）
        const bool rmbDown = (!useGameCamera) && viewportActive && input.isMouseDown(GLFW_MOUSE_BUTTON_RIGHT);

        const float mdx = (!useGameCamera && viewportActive) ? input.getMouseDeltaX() : 0.0f;
        const float mdy = (!useGameCamera && viewportActive) ? input.getMouseDeltaY() : 0.0f;
        const float scrollY = (!useGameCamera && viewportActive) ? input.getScrollDeltaY() : 0.0f;

        const bool keyW = (!useGameCamera) && viewportActive && input.isKeyDown(GLFW_KEY_W);
        const bool keyA = (!useGameCamera) && viewportActive && input.isKeyDown(GLFW_KEY_A);
        const bool keyS = (!useGameCamera) && viewportActive && input.isKeyDown(GLFW_KEY_S);
        const bool keyD = (!useGameCamera) && viewportActive && input.isKeyDown(GLFW_KEY_D);
        const bool keyQ = (!useGameCamera) && viewportActive && input.isKeyDown(GLFW_KEY_Q);
        const bool keyE = (!useGameCamera) && viewportActive && input.isKeyDown(GLFW_KEY_E);

        // 3) 更新 Editor 预览相机（游戏相机不吃 Editor 输入）
        if (!useGameCamera)
        {
            m_Camera.update(dt, viewportActive, rmbDown, mdx, mdy,
                keyW, keyA, keyS, keyD, keyQ, keyE,
                scrollY);
        }

        // 4) 选择 ViewProjection：EditorCamera 或 Scene Primary Camera
        glm::mat4 viewProj(1.0f);

        const float aspect = (viewportSize.y > 0.0f) ? (viewportSize.x / viewportSize.y) : 1.0f;

        if (useGameCamera)
        {
            if (!m_Scene)
            {
                // 没有场景：退回 editor camera
                viewProj = m_Camera.getViewProj();
            }
            else
            {
                // Scene 相机优先；若没找到 Primary Camera，则退回 editor camera
                if (!TryGetSceneViewProj(*m_Scene, aspect, viewProj))
                    viewProj = m_Camera.getViewProj();
            }
        }
        else
        {
            // Editor 预览
            viewProj = m_Camera.getViewProj();
        }

        // 5) 渲染到 FBO
        m_SceneFB->bind();
        RenderCommand::setViewport(0, 0, m_SceneFB->getWidth(), m_SceneFB->getHeight());

        Renderer::beginFrame({ 0.1f, 0.1f, 0.12f, 1.0f });

        // 6) 从 Scene 读取可渲染实体并绘制 cube
        if (m_Scene)
        {
            auto& registry = m_Scene->GetRegistry();
            auto renderView = registry.view<Hybrid::TransformComponent, Hybrid::MeshRendererComponent>();

            m_CubeShader->bind();
            m_CubeShader->setMat4("u_ViewProjection", viewProj);

            for (auto e : renderView)
            {
                const auto& tr = renderView.get<Hybrid::TransformComponent>(e);
                const auto& mr = renderView.get<Hybrid::MeshRendererComponent>(e);

                // 当前版本只处理 Primitive==0 的内建立方体
                if (mr.Primitive != 0)
                    continue;

                glm::mat4 model = BuildModel(tr);
                m_CubeShader->setMat4("u_Model", model);

                Renderer::submit(m_CubeVAO, m_CubeShader);
            }
        }

        Renderer::endFrame();
        m_SceneFB->unbind();

        // 7) 清屏默认帧缓冲（防 UI 拖影）
        GLFWwindow* window = static_cast<GLFWwindow*>(glfwWindowHandle);
        int display_w = 0, display_h = 0;
        glfwGetFramebufferSize(window, &display_w, &display_h);

        RenderCommand::setViewport(0, 0, display_w, display_h);
        RenderCommand::setClearColor({ 0.08f, 0.08f, 0.09f, 1.0f });
        RenderCommand::clear();
    }



    
}

