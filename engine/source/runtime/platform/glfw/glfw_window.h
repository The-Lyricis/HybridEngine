#pragma once

#include "runtime/core/platform/window.h"

struct GLFWwindow;

namespace Hybrid
{
    class GlfwWindow final : public IWindow
    {
    public:
        ~GlfwWindow() override;

        bool initialize(const WindowDesc& desc, std::string& error) override;
        void shutdown() override;
        void pollEvents() override;
        bool shouldClose() const override;
        void setShouldClose(bool close) override;
        FramebufferSize framebufferSize() const override;
        ContentScale contentScale() const override;
        bool isFocused() const override;
        void setCursorMode(CursorMode mode) override;
        CursorMode cursorMode() const override { return m_cursor_mode; }
        GraphicsBackend graphicsBackend() const override { return m_graphics_backend; }
        NativeWindowHandle nativeHandle() const override;
        void setEventCallback(EventCallback callback) override;
        bool makeGraphicsContextCurrent(std::string& error) override;
        GraphicsProcAddress graphicsProcAddress(const char* name) const override;
        void setSwapInterval(int interval) override;
        void swapBuffers() override;
        void* backendWindowHandle() const override;

    private:
        void installCallbacks();

        GLFWwindow* m_window = nullptr;
        EventCallback m_event_callback;
        CursorMode m_cursor_mode = CursorMode::Normal;
        GraphicsBackend m_graphics_backend = GraphicsBackend::OpenGL;
        bool m_glfw_initialized = false;
    };
} // namespace Hybrid
