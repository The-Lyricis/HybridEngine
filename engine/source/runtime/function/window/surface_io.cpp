#include <runtime/function/window/surface_io.h>

#include "runtime/core/event/application_event.h"
#include "runtime/core/event/input_event.h"

namespace Hybrid
{
    SurfaceIO::SurfaceIO(GLFWwindow* window)
        : m_window(window)
    {
        m_cb_data.self = this;
        glfwSetWindowUserPointer(m_window, &m_cb_data);
        installGlfwCallbacks();
    }

    void SurfaceIO::installGlfwCallbacks()
    {
        glfwSetWindowSizeCallback(m_window, [](GLFWwindow* window, int width, int height) {
            auto* cb_data = static_cast<CallbackData*>(glfwGetWindowUserPointer(window));
            if (!cb_data || !cb_data->self || !cb_data->self->m_on_event) return;
            WindowResizeEvent event(width, height);
            cb_data->self->m_on_event(event);
        });

        glfwSetWindowCloseCallback(m_window, [](GLFWwindow* window) {
            auto* cb_data = static_cast<CallbackData*>(glfwGetWindowUserPointer(window));
            if (!cb_data || !cb_data->self || !cb_data->self->m_on_event) return;
            WindowCloseEvent event;
            cb_data->self->m_on_event(event);
        });

        glfwSetWindowFocusCallback(m_window, [](GLFWwindow* window, int focused) {
            auto* cb_data = static_cast<CallbackData*>(glfwGetWindowUserPointer(window));
            if (!cb_data || !cb_data->self || !cb_data->self->m_on_event) return;
            if (focused)
            {
                WindowFocusEvent event;
                cb_data->self->m_on_event(event);
            }
            else
            {
                WindowLostFocusEvent event;
                cb_data->self->m_on_event(event);
            }
        });

        glfwSetKeyCallback(m_window, [](GLFWwindow* window, int key, int scancode, int action, int mods) {
            auto* cb_data = static_cast<CallbackData*>(glfwGetWindowUserPointer(window));
            if (!cb_data || !cb_data->self || !cb_data->self->m_on_event) return;
            (void)scancode;
            (void)mods;

            switch (action)
            {
            case GLFW_PRESS:
            {
                KeyPressedEvent event(key, false);
                cb_data->self->m_on_event(event);
                break;
            }
            case GLFW_RELEASE:
            {
                KeyReleasedEvent event(key);
                cb_data->self->m_on_event(event);
                break;
            }
            case GLFW_REPEAT:
            {
                KeyPressedEvent event(key, true);
                cb_data->self->m_on_event(event);
                break;
            }
            default:
                break;
            }
        });

        glfwSetCharCallback(m_window, [](GLFWwindow* window, unsigned int codepoint) {
            auto* cb_data = static_cast<CallbackData*>(glfwGetWindowUserPointer(window));
            if (!cb_data || !cb_data->self || !cb_data->self->m_on_event) return;
            KeyTypedEvent event(static_cast<int>(codepoint));
            cb_data->self->m_on_event(event);
        });

        glfwSetCursorPosCallback(m_window, [](GLFWwindow* window, double x, double y) {
            auto* cb_data = static_cast<CallbackData*>(glfwGetWindowUserPointer(window));
            if (!cb_data || !cb_data->self || !cb_data->self->m_on_event) return;
            MouseMovedEvent event(static_cast<float>(x), static_cast<float>(y));
            cb_data->self->m_on_event(event);
        });

        glfwSetMouseButtonCallback(m_window, [](GLFWwindow* window, int button, int action, int mods) {
            auto* cb_data = static_cast<CallbackData*>(glfwGetWindowUserPointer(window));
            if (!cb_data || !cb_data->self || !cb_data->self->m_on_event) return;
            (void)mods;

            switch (action)
            {
            case GLFW_PRESS:
            {
                MouseButtonPressedEvent event(button);
                cb_data->self->m_on_event(event);
                break;
            }
            case GLFW_RELEASE:
            {
                MouseButtonReleasedEvent event(button);
                cb_data->self->m_on_event(event);
                break;
            }
            default:
                break;
            }
        });

        glfwSetScrollCallback(m_window, [](GLFWwindow* window, double xoffset, double yoffset) {
            auto* cb_data = static_cast<CallbackData*>(glfwGetWindowUserPointer(window));
            if (!cb_data || !cb_data->self || !cb_data->self->m_on_event) return;
            MouseScrolledEvent event(static_cast<float>(xoffset), static_cast<float>(yoffset));
            cb_data->self->m_on_event(event);
        });
    }

    void SurfaceIO::setFocusMode(bool enable)
    {
        m_focus_mode = enable;
        glfwSetInputMode(m_window, GLFW_CURSOR, enable ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    }
}
