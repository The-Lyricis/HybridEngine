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

        glfwMakeContextCurrent(m_window);
        glfwSwapInterval(1); // 开启垂直同步

        // --- 核心步骤：初始化 SurfaceIO ---
        m_surface_io = std::make_shared<SurfaceIO>(m_window);

        // 关键：将 SurfaceIO 存入窗口的 UserPointer
        // 这样 installGlfwCallbacks 里的 static_cast 才能拿到数据
        // 注意：这里我们存的是 m_cb_data 的地址，它在 SurfaceIO 内部定义
        // 假设 SurfaceIO 构造函数里已经处理了：glfwSetWindowUserPointer(m_window, &m_cb_data);
    }

    void WindowSystem::cleanup() {
        if (m_window) {
            glfwDestroyWindow(m_window);
            glfwTerminate();
        }
    }
}
