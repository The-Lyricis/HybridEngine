#include "opengl_context.h"
#include <glad/gl.h>
#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>
#include <cstdio>

namespace Hybrid {

    OpenGLContext::OpenGLContext(GLFWwindow* window)
        : m_Window(window) {}

    void OpenGLContext::init() {
        glfwMakeContextCurrent(m_Window);
        int status = gladLoadGL(glfwGetProcAddress);
        if (status == 0) {
            std::fprintf(stderr, "gladLoadGL failed\n");
            return;
        }
        glfwSwapInterval(1); // vsync on by default
    }

    void OpenGLContext::swapBuffers() {
        glfwSwapBuffers(m_Window);
    }

} // namespace Hybrid
