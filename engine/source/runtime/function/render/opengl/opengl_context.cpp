#include "opengl_context.h"
#include <glad/gl.h>
#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>
#include <cstdio>
#include "runtime/core/base/macro.h"

namespace Hybrid {

    OpenGLContext::OpenGLContext(GLFWwindow* window)
        : m_Window(window) {}

    void OpenGLContext::init() {
        glfwMakeContextCurrent(m_Window);
        int status = gladLoadGL(glfwGetProcAddress);
        if (status == 0) {
             HBD_CORE_ERROR("Failed to initialize OpenGL context");
            return;
        }
        glfwSwapInterval(1); // vsync on by default
    }

    void OpenGLContext::swapBuffers() {
        glfwSwapBuffers(m_Window);
    }

} // namespace Hybrid
