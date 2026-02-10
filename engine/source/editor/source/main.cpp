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

    void OnEvent(Hybrid::Event & e) override
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
// 计算 dt（秒）
static float CalcDeltaTime()
{
    static double s_Last = 0.0;
    const double now = glfwGetTime();
    if (s_Last == 0.0) s_Last = now;
    const float dt = static_cast<float>(now - s_Last);
    s_Last = now;
    return dt;
}

int main(int argc, char** argv)
{
    std::cout << "Starting Hybrid Engine..." << std::endl;

    Hybrid::LogSystem::initialize();

    Hybrid::LayerStack stack;
    Hybrid::InputLayer input_layer;
    TestLayer layer("TestLayer");
    OverlayLayer overlay("Overlay");
    stack.PushLayer(&layer);
    stack.PushOverlay(&overlay);
    stack.PushLayer(&input_layer);

    auto window_system = std::make_shared<Hybrid::WindowSystem>();
    window_system->initialize(1280, 720, "Hybrid Engine Editor");

    GLFWwindow* window = window_system->getGLFWWindow();
    if (!window) {
        HBD_CORE_ERROR("GLFW window is null.");
        window_system->cleanup();
        Hybrid::LogSystem::shutdown();
        return -1;
    }

    glfwMakeContextCurrent(window);

    const int ver = gladLoadGL(glfwGetProcAddress);
    if (ver == 0) {
        HBD_CORE_ERROR("gladLoadGL failed (returned 0).");
        window_system->cleanup();
        Hybrid::LogSystem::shutdown();
        return -1;
    }

    // 事件回调：InputLayer sample + LayerStack dispatch
    auto surface_io = window_system->getSurfaceIO();
    surface_io->registerOnEventFunc([&](Hybrid::Event& e) {
        input_layer.onEvent(e); // phase1: sample（采样输入到 InputState）
        for (auto it = stack.rbegin(); it != stack.rend(); ++it)
        {
            (*it)->onEvent(e);    // phase2: dispatch（可 Handled 中断）
            if (e.Handled) break;
        }
        });

    auto editor_ui = std::make_shared<Hybrid::EditorUI>();
    editor_ui->initialize(window);

    Hybrid::RenderSystem render_system;
    render_system.initialize(window);

    while (!window_system->shouldClose()) {

        const float dt = CalcDeltaTime();

        // 每帧开始：清空边沿状态 + 清空 delta（必须在 pollEvents 之前）
        input_layer.onUpdate(dt);        // 内部应调用 m_state.NewFrame()

        // 触发 GLFW 回调，回调里会把输入累加进 InputState
        window_system->pollEvents();

        // UI begin
        editor_ui->beginFrame();
        editor_ui->drawPanels();

        // Viewport 先布局（拿 size / hovered / focused）
        editor_ui->drawViewport(render_system.getSceneColorTexture());

        const bool viewportActive = editor_ui->isViewportHovered() && editor_ui->isViewportFocused();

        // M3：RenderSystem 使用 InputState 驱动相机 + 渲染到 FBO
        render_system.renderFrame(
            editor_ui->getViewportSize(),
            window,
            dt,
            viewportActive,
            input_layer.getState()
        );

        editor_ui->endFrame();
        glfwSwapBuffers(window);
    }

    editor_ui->shutdown();
    window_system->cleanup();
    Hybrid::LogSystem::shutdown();
    return 0;
}
