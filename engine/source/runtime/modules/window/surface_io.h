#pragma once
#include <functional>
#include <GLFW/glfw3.h>

#include "runtime/core/event/event.h"

namespace Hybrid
{
    class SurfaceIO
    {
    public:
        using OnEventFunc = std::function<void(Event& e)>;

        explicit SurfaceIO(GLFWwindow* window);
        ~SurfaceIO() = default;

        void registerOnEventFunc(OnEventFunc f) { m_on_event = std::move(f); }

        void setFocusMode(bool enable);
        bool isFocusMode() const { return m_focus_mode; }
        GLFWwindow* getWindow() const { return m_window; }
    private:
        struct CallbackData
        {
            SurfaceIO* self;
        };

        void installGlfwCallbacks();

    private:
        GLFWwindow* m_window{ nullptr };
        bool m_focus_mode{ false };
        OnEventFunc m_on_event;
        CallbackData m_cb_data;
    };
}
