#pragma once
#include <GLFW/glfw3.h>
#include <memory>
#include <runtime/function/render/surface_io.h>


namespace Hybrid {
    class WindowSystem {
    public:
        WindowSystem() = default;
        ~WindowSystem() { cleanup(); }

        void initialize(int width, int height, const char* title);
        void pollEvents() { glfwPollEvents(); }
        bool shouldClose() { return glfwWindowShouldClose(m_window); }
        void cleanup();

        GLFWwindow* getGLFWWindow() { return m_window; }
        std::shared_ptr<SurfaceIO> getSurfaceIO() { return m_surface_io; }

    private:
        GLFWwindow* m_window{ nullptr };
        std::shared_ptr<SurfaceIO> m_surface_io{ nullptr };
    };
}
