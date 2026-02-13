#include <runtime/function/window/window_system.h>
#include <stdexcept>

namespace Hybrid {
    void WindowSystem::initialize(int width, int height, const char* title) {
        if (!glfwInit()) {
            throw std::runtime_error("Failed to initialize GLFW");
        }

        m_window = glfwCreateWindow(width, height, title, nullptr, nullptr);
        if (!m_window) {
            glfwTerminate();
            throw std::runtime_error("Failed to create GLFW window");
        }

        // SurfaceIO handles input callbacks; user pointer set inside SurfaceIO ctor.
        m_surface_io = std::make_shared<SurfaceIO>(m_window);
    }

    void WindowSystem::cleanup() {
        if (m_window) {
            glfwDestroyWindow(m_window);
            glfwTerminate();
            m_window = nullptr;
        }
    }
}
