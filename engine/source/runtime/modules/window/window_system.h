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

        void initialize(int width, int height, const char* title);
        void pollEvents() { glfwPollEvents(); }
        bool shouldClose() { return glfwWindowShouldClose(m_window); }
        void cleanup();

        GLFWwindow* getNativeWindow() { return m_window; }
        std::shared_ptr<SurfaceIO> getSurfaceIO() { return m_surface_io; }

    private:
        GLFWwindow* m_window{ nullptr };
        std::shared_ptr<SurfaceIO> m_surface_io{ nullptr };
    };
}

