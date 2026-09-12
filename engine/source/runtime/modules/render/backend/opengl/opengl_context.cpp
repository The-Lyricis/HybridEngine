#include "opengl_context.h"

#include <glad/gl.h>

#include "runtime/core/base/macro.h"
#include "runtime/core/platform/window.h"

namespace Hybrid
{
    namespace
    {
        constexpr const char* kOpenGLContextLogTag = "[OpenGLContext]";
    }

    GLContext::GLContext(IWindow& window)
        : m_Window(&window)
    {
    }

    void GLContext::init()
    {
        std::string error;
        if (!m_Window || !m_Window->makeGraphicsContextCurrent(error))
        {
            HBD_CORE_ERROR("{} initialize_failed reason={}", kOpenGLContextLogTag, error);
            return;
        }

        const int status = gladLoadGLUserPtr(
            [](void* user, const char* name) -> GLADapiproc
            {
                auto* window = static_cast<IWindow*>(user);
                return reinterpret_cast<GLADapiproc>(window->graphicsProcAddress(name));
            },
            m_Window);
        if (status == 0)
        {
            HBD_CORE_ERROR("{} initialize_failed reason=glad_load_failed", kOpenGLContextLogTag);
            return;
        }

        m_Window->setSwapInterval(1);
        HBD_CORE_INFO("{} initialize_completed vsync=1", kOpenGLContextLogTag);
    }

    void GLContext::swapBuffers()
    {
        if (m_Window)
            m_Window->swapBuffers();
    }
} // namespace Hybrid
