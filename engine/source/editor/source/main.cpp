#include <iostream>
#include <memory>

#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include "runtime/core/log/log_system.h"
#include "runtime/function/window/window_system.h"
#include "runtime/function/input/input_system.h"

#include "editor/include/editor_ui.h"
#include "runtime/function/render/render_system.h"

int main(int argc, char** argv) {
    std::cout << "Starting Hybrid Engine..." << std::endl;

    Hybrid::LogSystem::Init();

    auto window_system = std::make_shared<Hybrid::WindowSystem>();
    window_system->initialize(1280, 720, "Hybrid Engine Editor");

    GLFWwindow* window = window_system->getGLFWWindow();
    if (!window) {
        window_system->cleanup();
        Hybrid::LogSystem::Shutdown();
        return -1;
    }

    glfwMakeContextCurrent(window);

    const int ver = gladLoadGL(glfwGetProcAddress);
    if (ver == 0) {
        window_system->cleanup();
        Hybrid::LogSystem::Shutdown();
        return -1;
    }

    Hybrid::InputSystem::getInstance().initialize(*window_system->getSurfaceIO());

    auto editor_ui = std::make_shared<Hybrid::EditorUI>();
    editor_ui->initialize(window);

    Hybrid::RenderSystem render_system;
    render_system.initialize(window);

    while (!window_system->shouldClose()) {
        Hybrid::InputSystem::getInstance().tick();
        window_system->pollEvents();

        editor_ui->beginFrame();
        editor_ui->drawPanels();

        // UI 先布局 viewport，并记录 viewport size
        editor_ui->drawViewport(render_system.getSceneColorTexture());

        // RenderSystem 根据 viewport size 渲染，并清屏 backbuffer
        render_system.renderFrame(editor_ui->getViewportSize(), window);

        // 输出 UI 到 backbuffer（刚刚已清屏）
        editor_ui->endFrame();
        glfwSwapBuffers(window);
    }

    editor_ui->shutdown();
    window_system->cleanup();
    Hybrid::LogSystem::Shutdown();
    return 0;
}
