#include "runtime/platform/glfw/glfw_window.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#if defined(_WIN32)
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#elif defined(__APPLE__)
#define GLFW_EXPOSE_NATIVE_COCOA
#include <GLFW/glfw3native.h>
#endif

#include "runtime/core/event/application_event.h"
#include "runtime/core/event/input_event.h"

namespace Hybrid
{
    GlfwWindow::~GlfwWindow()
    {
        shutdown();
    }

    bool GlfwWindow::initialize(const WindowDesc& desc, std::string& error)
    {
        if (m_window)
            return true;

        if (!glfwInit())
        {
            error = "failed to initialize GLFW";
            return false;
        }
        m_glfw_initialized = true;
        m_graphics_backend = desc.graphics_backend;

        glfwWindowHint(GLFW_VISIBLE, desc.visible ? GLFW_TRUE : GLFW_FALSE);
        if (desc.graphics_backend == GraphicsBackend::OpenGL)
        {
#if defined(__APPLE__)
            glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
            glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
            glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#else
            glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
            glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#endif
        }
        else
        {
            glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        }

        m_window = glfwCreateWindow(static_cast<int>(desc.width),
                                    static_cast<int>(desc.height),
                                    desc.title.c_str(),
                                    nullptr,
                                    nullptr);
        glfwDefaultWindowHints();
        if (!m_window)
        {
            error = "failed to create GLFW window";
            shutdown();
            return false;
        }

        glfwSetWindowUserPointer(m_window, this);
        installCallbacks();
        return true;
    }

    void GlfwWindow::shutdown()
    {
        if (m_window)
        {
            glfwDestroyWindow(m_window);
            m_window = nullptr;
        }
        if (m_glfw_initialized)
        {
            glfwTerminate();
            m_glfw_initialized = false;
        }
        m_event_callback = {};
        m_cursor_mode = CursorMode::Normal;
        m_graphics_backend = GraphicsBackend::OpenGL;
    }

    void GlfwWindow::pollEvents()
    {
        glfwPollEvents();
    }

    bool GlfwWindow::shouldClose() const
    {
        return m_window && glfwWindowShouldClose(m_window) != 0;
    }

    void GlfwWindow::setShouldClose(bool close)
    {
        if (m_window)
            glfwSetWindowShouldClose(m_window, close ? GLFW_TRUE : GLFW_FALSE);
    }

    FramebufferSize GlfwWindow::framebufferSize() const
    {
        int width = 0;
        int height = 0;
        if (m_window)
            glfwGetFramebufferSize(m_window, &width, &height);
        return {static_cast<uint32_t>(width > 0 ? width : 0),
                static_cast<uint32_t>(height > 0 ? height : 0)};
    }

    ContentScale GlfwWindow::contentScale() const
    {
        ContentScale scale{};
        if (m_window)
            glfwGetWindowContentScale(m_window, &scale.x, &scale.y);
        return scale;
    }

    bool GlfwWindow::isFocused() const
    {
        return m_window && glfwGetWindowAttrib(m_window, GLFW_FOCUSED) != 0;
    }

    void GlfwWindow::setCursorMode(CursorMode mode)
    {
        m_cursor_mode = mode;
        if (!m_window)
            return;

        int glfw_mode = GLFW_CURSOR_NORMAL;
        if (mode == CursorMode::Hidden)
            glfw_mode = GLFW_CURSOR_HIDDEN;
        else if (mode == CursorMode::Captured)
            glfw_mode = GLFW_CURSOR_DISABLED;
        glfwSetInputMode(m_window, GLFW_CURSOR, glfw_mode);
    }

    NativeWindowHandle GlfwWindow::nativeHandle() const
    {
        if (!m_window)
            return {};
#if defined(_WIN32)
        return {NativeWindowKind::Win32, static_cast<void*>(glfwGetWin32Window(m_window)), nullptr};
#elif defined(__APPLE__)
        return {NativeWindowKind::Cocoa,
                reinterpret_cast<void*>(glfwGetCocoaWindow(m_window)),
                reinterpret_cast<void*>(glfwGetCocoaView(m_window))};
#else
        return {};
#endif
    }

    void GlfwWindow::setEventCallback(EventCallback callback)
    {
        m_event_callback = std::move(callback);
    }

    bool GlfwWindow::makeGraphicsContextCurrent(std::string& error)
    {
        if (!m_window || m_graphics_backend != GraphicsBackend::OpenGL)
        {
            error = "window does not own an OpenGL context";
            return false;
        }
        glfwMakeContextCurrent(m_window);
        return true;
    }

    IWindow::GraphicsProcAddress GlfwWindow::graphicsProcAddress(const char* name) const
    {
        return reinterpret_cast<GraphicsProcAddress>(glfwGetProcAddress(name));
    }

    void GlfwWindow::setSwapInterval(int interval)
    {
        if (m_graphics_backend == GraphicsBackend::OpenGL)
            glfwSwapInterval(interval);
    }

    void GlfwWindow::swapBuffers()
    {
        if (m_window && m_graphics_backend == GraphicsBackend::OpenGL)
            glfwSwapBuffers(m_window);
    }

    void* GlfwWindow::backendWindowHandle() const
    {
        return m_window;
    }

    void GlfwWindow::installCallbacks()
    {
        glfwSetWindowSizeCallback(m_window, [](GLFWwindow* window, int width, int height) {
            auto* self = static_cast<GlfwWindow*>(glfwGetWindowUserPointer(window));
            if (!self || !self->m_event_callback) return;
            WindowResizeEvent event(width, height);
            self->m_event_callback(event);
        });
        glfwSetWindowCloseCallback(m_window, [](GLFWwindow* window) {
            auto* self = static_cast<GlfwWindow*>(glfwGetWindowUserPointer(window));
            if (!self || !self->m_event_callback) return;
            WindowCloseEvent event;
            self->m_event_callback(event);
        });
        glfwSetWindowFocusCallback(m_window, [](GLFWwindow* window, int focused) {
            auto* self = static_cast<GlfwWindow*>(glfwGetWindowUserPointer(window));
            if (!self || !self->m_event_callback) return;
            if (focused)
            {
                WindowFocusEvent event;
                self->m_event_callback(event);
            }
            else
            {
                WindowLostFocusEvent event;
                self->m_event_callback(event);
            }
        });
        glfwSetKeyCallback(m_window, [](GLFWwindow* window, int key, int, int action, int) {
            auto* self = static_cast<GlfwWindow*>(glfwGetWindowUserPointer(window));
            if (!self || !self->m_event_callback) return;
            if (action == GLFW_PRESS)
            {
                KeyPressedEvent event(key, false);
                self->m_event_callback(event);
            }
            else if (action == GLFW_REPEAT)
            {
                KeyPressedEvent event(key, true);
                self->m_event_callback(event);
            }
            else if (action == GLFW_RELEASE)
            {
                KeyReleasedEvent event(key);
                self->m_event_callback(event);
            }
        });
        glfwSetCharCallback(m_window, [](GLFWwindow* window, unsigned int codepoint) {
            auto* self = static_cast<GlfwWindow*>(glfwGetWindowUserPointer(window));
            if (!self || !self->m_event_callback) return;
            KeyTypedEvent event(static_cast<int>(codepoint));
            self->m_event_callback(event);
        });
        glfwSetCursorPosCallback(m_window, [](GLFWwindow* window, double x, double y) {
            auto* self = static_cast<GlfwWindow*>(glfwGetWindowUserPointer(window));
            if (!self || !self->m_event_callback) return;
            MouseMovedEvent event(static_cast<float>(x), static_cast<float>(y));
            self->m_event_callback(event);
        });
        glfwSetMouseButtonCallback(m_window, [](GLFWwindow* window, int button, int action, int) {
            auto* self = static_cast<GlfwWindow*>(glfwGetWindowUserPointer(window));
            if (!self || !self->m_event_callback) return;
            if (action == GLFW_PRESS)
            {
                MouseButtonPressedEvent event(button);
                self->m_event_callback(event);
            }
            else if (action == GLFW_RELEASE)
            {
                MouseButtonReleasedEvent event(button);
                self->m_event_callback(event);
            }
        });
        glfwSetScrollCallback(m_window, [](GLFWwindow* window, double x, double y) {
            auto* self = static_cast<GlfwWindow*>(glfwGetWindowUserPointer(window));
            if (!self || !self->m_event_callback) return;
            MouseScrolledEvent event(static_cast<float>(x), static_cast<float>(y));
            self->m_event_callback(event);
        });
    }

    std::unique_ptr<IWindow> CreatePlatformWindow()
    {
        return std::make_unique<GlfwWindow>();
    }
} // namespace Hybrid
