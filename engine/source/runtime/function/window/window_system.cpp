#include <runtime/function/window/window_system.h>
#include <stdexcept>

#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include <windows.h>
#endif

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

#ifdef _WIN32
        // 同步设置任务栏/标题栏图标：从内置资源 APP_ICON 加载
        HWND hwnd = glfwGetWin32Window(m_window);
        if (hwnd) {
            constexpr int APP_ICON_ID = 101; // 与 resource/app_icon.rc 定义保持一致
            HICON hIcon = static_cast<HICON>(LoadImage(
                GetModuleHandle(nullptr),
                MAKEINTRESOURCE(APP_ICON_ID),
                IMAGE_ICON,
                0, 0,
                LR_DEFAULTSIZE | LR_SHARED));
            if (hIcon) {
                SendMessage(hwnd, WM_SETICON, ICON_BIG,   reinterpret_cast<LPARAM>(hIcon));
                SendMessage(hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(hIcon));
            }
        }
#endif

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
