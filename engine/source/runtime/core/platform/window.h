#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#include "runtime/core/event/event.h"
#include "runtime/core/platform/graphics_backend.h"

namespace Hybrid
{
    enum class NativeWindowKind
    {
        None = 0,
        Win32,
        Cocoa,
        X11,
        Wayland,
    };

    struct NativeWindowHandle
    {
        NativeWindowKind kind = NativeWindowKind::None;
        void* window = nullptr;
        void* view = nullptr;

        explicit operator bool() const { return window != nullptr; }
    };

    enum class CursorMode
    {
        Normal = 0,
        Hidden,
        Captured,
    };

    struct FramebufferSize
    {
        uint32_t width = 0;
        uint32_t height = 0;
    };

    struct ContentScale
    {
        float x = 1.0f;
        float y = 1.0f;
    };

    struct WindowDesc
    {
        uint32_t width = 1280;
        uint32_t height = 720;
        std::string title = "Hybrid Engine";
        bool visible = true;
        GraphicsBackend graphics_backend = GraphicsBackend::OpenGL;
    };

    class IWindow
    {
    public:
        using EventCallback = std::function<void(Event&)>;
        using GraphicsProcAddress = void (*)();

        virtual ~IWindow() = default;
        virtual bool initialize(const WindowDesc& desc, std::string& error) = 0;
        virtual void shutdown() = 0;
        virtual void pollEvents() = 0;
        virtual bool shouldClose() const = 0;
        virtual void setShouldClose(bool close) = 0;
        virtual FramebufferSize framebufferSize() const = 0;
        virtual ContentScale contentScale() const = 0;
        virtual bool isFocused() const = 0;
        virtual void setCursorMode(CursorMode mode) = 0;
        virtual CursorMode cursorMode() const = 0;
        virtual GraphicsBackend graphicsBackend() const = 0;
        virtual NativeWindowHandle nativeHandle() const = 0;
        virtual void setEventCallback(EventCallback callback) = 0;
        virtual bool makeGraphicsContextCurrent(std::string& error) = 0;
        virtual GraphicsProcAddress graphicsProcAddress(const char* name) const = 0;
        virtual void setSwapInterval(int interval) = 0;
        virtual void swapBuffers() = 0;

        // Restricted escape hatch for ImGui platform adapters.
        virtual void* backendWindowHandle() const = 0;
    };

    std::unique_ptr<IWindow> CreatePlatformWindow();
} // namespace Hybrid
