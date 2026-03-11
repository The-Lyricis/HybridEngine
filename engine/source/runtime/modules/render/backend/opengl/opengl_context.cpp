#include "opengl_context.h"
#include <glad/gl.h>
#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>
#include <cstdio>
#include "runtime/core/base/macro.h"

namespace Hybrid {
    namespace
    {
        constexpr const char* kOpenGLContextLogTag = "[OpenGLContext]";
    } // namespace

    GLContext::GLContext(GLFWwindow* window)
        : m_Window(window) {}

    void GLContext::init() {
        glfwMakeContextCurrent(m_Window);
        int status = gladLoadGL(glfwGetProcAddress);
        if (status == 0) {
             HBD_CORE_ERROR("{} initialize_failed reason=glad_load_failed", kOpenGLContextLogTag);
            return;
        }
        glfwSwapInterval(1); // vsync on by default
        HBD_CORE_INFO("{} initialize_completed vsync=1", kOpenGLContextLogTag);
    }

    void GLContext::swapBuffers() {
        glfwSwapBuffers(m_Window);
    }

} // namespace Hybrid
