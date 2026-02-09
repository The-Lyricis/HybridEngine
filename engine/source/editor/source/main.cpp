#include <iostream>
#include <memory>

#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include "runtime/core/log/log_system.h"
#include "runtime/function/window/window_system.h"

#include "runtime/function/render/renderer.h"
#include "runtime/function/render/render_command.h"
#include "runtime/function/render/vertex_array.h"
#include "runtime/function/render/shader.h"

#include "editor/include/editor_ui.h"
#include "runtime/function/render/render_system.h"

class TestLayer : public Hybrid::Layer
{
public:
    using Layer::Layer;

    void OnEvent(Hybrid::Event& e) override
    {
        std::cout << "[TestLayer] " << e.ToString() << "\n";
        if (e.GetEventType() == Hybrid::EventType::KeyPressed)
            e.Handled = true;
    }
};

class OverlayLayer : public Hybrid::Layer
{
public:
    using Layer::Layer;

    void OnEvent(Hybrid::Event& e) override
    {
        std::cout << "[Overlay] " << e.ToString() << "\n";
    }
};

int main(int argc, char** argv)
{
    std::cout << "Starting Hybrid Engine..." << std::endl;

    Hybrid::LogSystem::Init();

    Hybrid::LayerStack stack;
    TestLayer layer("TestLayer");
    OverlayLayer overlay("Overlay");
    stack.PushLayer(&layer);
    stack.PushOverlay(&overlay);

    auto window_system = std::make_shared<Hybrid::WindowSystem>();
    window_system->initialize(1280, 720, "Hybrid Engine Editor");

    GLFWwindow* window = window_system->getGLFWWindow();
    if (!window) {
        HBD_CORE_ERROR("GLFW window is null.");
        window_system->cleanup();
        Hybrid::LogSystem::Shutdown();
        return -1;
    }

    glfwMakeContextCurrent(window);

    const int ver = gladLoadGL(glfwGetProcAddress);
    if (ver == 0) {
        HBD_CORE_ERROR("gladLoadGL failed (returned 0).");
        window_system->cleanup();
        Hybrid::LogSystem::Shutdown();
        return -1;
    }

    const char* glVersion = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    HBD_CORE_INFO("OpenGL Version: {}", glVersion ? glVersion : "null");

    // 输入系统初始化
    Hybrid::InputSystem::getInstance().initialize(*window_system->getSurfaceIO());

    auto editor_ui = std::make_shared<Hybrid::EditorUI>();
    editor_ui->initialize(window);

    // Renderer 初始化（只做一次）
    Hybrid::Renderer::Init();

    // ---------- 创建三角形资源（只创建一次） ----------
    static float s_TriVertices[] = {
        // x,    y,    z,    r,   g,   b,   a
        -0.5f, -0.5f, 0.0f, 1.f, 0.f, 0.f, 1.f,
         0.5f, -0.5f, 0.0f, 0.f, 1.f, 0.f, 1.f,
         0.0f,  0.5f, 0.0f, 0.f, 0.f, 1.f, 1.f,
    };
    static uint32_t s_TriIndices[] = { 0, 1, 2 };

    auto vao = std::make_shared<Hybrid::VertexArray>();
    auto vb = std::make_shared<Hybrid::VertexBuffer>(s_TriVertices, sizeof(s_TriVertices));
    auto ib = std::make_shared<Hybrid::IndexBuffer>(s_TriIndices, 3);

    vao->SetVertexBuffer(vb);
    vao->SetIndexBuffer(ib);

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

    auto shader = std::make_shared<Hybrid::Shader>(vs, fs);

    HBD_CORE_INFO("Hybrid Engine Initialized.");

    // ---------------- 主循环 ----------------
    while (!window_system->shouldClose()) {
        // 输入系统：清空边沿并收集新事件
        Hybrid::InputSystem::getInstance().tick();
        window_system->pollEvents();

        editor_ui->beginFrame();
        editor_ui->drawPanels();

        // 2) Scene render：设置 viewport + 清屏 + 画三角形
        int display_w = 0, display_h = 0;
        glfwGetFramebufferSize(window, &display_w, &display_h);

        Hybrid::RenderCommand::SetViewport(0, 0, display_w, display_h);

        // RenderSystem 根据 viewport size 渲染，并清屏 backbuffer
        render_system.renderFrame(editor_ui->getViewportSize(), window);

        // 3) UI end：把 ImGui draw data 画到当前 backbuffer 上
        editor_ui->endFrame();

        // 4) Swap buffers（必须由 main 控制，避免 UI 内部又清屏/乱序）
        glfwSwapBuffers(window);
    }

    editor_ui->shutdown();
    window_system->cleanup();
    Hybrid::LogSystem::Shutdown();
    return 0;
}
