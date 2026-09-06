#pragma once
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <memory>
#include <runtime/modules/window/surface_io.h>


namespace Hybrid {
    class WindowSystem {
    public:
        WindowSystem() = default;
        ~WindowSystem() { cleanup(); }

        void initialize(int width, int height, const char* title, bool visible = true);
        void pollEvents() { glfwPollEvents(); }
        bool shouldClose() const { return m_window && glfwWindowShouldClose(m_window); }
        void setShouldClose(bool close) { if (m_window) glfwSetWindowShouldClose(m_window, close ? GLFW_TRUE : GLFW_FALSE); }
        void cleanup();

        GLFWwindow* getNativeWindow() { return m_window; }
        std::shared_ptr<SurfaceIO> getSurfaceIO() { return m_surface_io; }

    private:
        GLFWwindow* m_window{ nullptr };
        std::shared_ptr<SurfaceIO> m_surface_io{ nullptr };
    };
}

