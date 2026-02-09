#include <iostream>
#include <memory>
#include <string>

#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include "runtime/core/base/macro.h"
#include "runtime/core/log/log_system.h"
#include "runtime/function/window/window_system.h"

#include "runtime/core/event/event.h"
#include "runtime/core/event/layer.h"
#include "runtime/core/event/application_event.h"
#include "runtime/core/event/input_event.h"

#include "runtime/function/render/renderer.h"
#include "runtime/function/render/render_command.h"
#include "runtime/function/render/vertex_array.h"
#include "runtime/function/render/shader.h"

#include "editor/include/editor_ui.h"

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
    if (!window)
    {
        HBD_CORE_ERROR("GLFW window is null.");
        window_system->cleanup();
        Hybrid::LogSystem::Shutdown();
        return -1;
    }

    glfwMakeContextCurrent(window);

    const int ver = gladLoadGL(glfwGetProcAddress);
    if (ver == 0)
    {
        HBD_CORE_ERROR("gladLoadGL failed (returned 0).");
        window_system->cleanup();
        Hybrid::LogSystem::Shutdown();
        return -1;
    }

    const char* glVersion = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    HBD_CORE_INFO("OpenGL Version: {}", glVersion ? glVersion : "null");

    auto surface_io = window_system->getSurfaceIO();
    surface_io->registerOnEventFunc([&stack](Hybrid::Event& e) {
        for (auto it = stack.rbegin(); it != stack.rend(); ++it)
        {
            (*it)->OnEvent(e);
            if (e.Handled)
                break;
        }
    });

    auto editor_ui = std::make_shared<Hybrid::EditorUI>();
    editor_ui->initialize(window);

    Hybrid::Renderer::Init();

    static float s_TriVertices[] = {
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

    while (!window_system->shouldClose())
    {
        window_system->pollEvents();

        editor_ui->beginFrame();
        editor_ui->drawPanels();

        int display_w = 0, display_h = 0;
        glfwGetFramebufferSize(window, &display_w, &display_h);

        Hybrid::RenderCommand::SetViewport(0, 0, display_w, display_h);

        Hybrid::Renderer::BeginFrame({ 0.1f, 0.1f, 0.12f, 1.0f });
        Hybrid::Renderer::Submit(vao, shader);
        Hybrid::Renderer::EndFrame();

        editor_ui->endFrame();

        glfwSwapBuffers(window);
    }

    editor_ui->shutdown();
    window_system->cleanup();
    Hybrid::LogSystem::Shutdown();
    return 0;
}
