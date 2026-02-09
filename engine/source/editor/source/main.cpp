#include <iostream>
#include <memory>

#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include "runtime/core/log/log_system.h"
#include "runtime/function/window/window_system.h"

#include "runtime/core/event/event.h"
#include "runtime/core/event/layer.h"
#include "runtime/core/event/application_event.h"
#include "runtime/core/event/input_event.h"

#include "runtime/function/input/input_layer.h"

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
    Hybrid::InputLayer input_layer;
    // TestLayer layer("TestLayer");
    // OverlayLayer overlay("Overlay");
    stack.PushLayer(&input_layer);
    // stack.PushLayer(&layer);
    // stack.PushOverlay(&overlay);

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
    auto surface_io = window_system->getSurfaceIO();
    surface_io->registerOnEventFunc([&](Hybrid::Event& e) {
        input_layer.OnEvent(e); // phase 1: sample (ignore Handled)
        for (auto it = stack.rbegin(); it != stack.rend(); ++it)
        {
            (*it)->OnEvent(e); // phase 2: dispatch (Handled can break)
            if (e.Handled)
                break;
        }
    });

    auto editor_ui = std::make_shared<Hybrid::EditorUI>();
    editor_ui->initialize(window);

    Hybrid::RenderSystem render_system;
    render_system.initialize(window);


    while (!window_system->shouldClose()) {

        input_layer.OnUpdate(0.0f);
        window_system->pollEvents();

        editor_ui->beginFrame();
        editor_ui->drawPanels();
        // UI 先布局 viewport，并记录 viewport size
        editor_ui->drawViewport(render_system.getSceneColorTexture());
        // RenderSystem 根据 viewport size 渲染，并清屏 backbuffer
        render_system.renderFrame(editor_ui->getViewportSize(), window);

        editor_ui->endFrame();

        glfwSwapBuffers(window);
    }

    editor_ui->shutdown();
    window_system->cleanup();
    Hybrid::LogSystem::Shutdown();
    return 0;
}
