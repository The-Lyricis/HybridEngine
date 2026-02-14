#pragma once
#include "../graphics_context.h"

// Forward declaration to avoid pulling GL headers into the header.
struct GLFWwindow;

namespace Hybrid {

    // GLContext: owns a GLFW-backed GL context and swap chain.
    class GLContext final : public GraphicsContext {
    public:
        explicit GLContext(GLFWwindow* window);

        void init() override;
        void swapBuffers() override;

    private:
        GLFWwindow* m_Window = nullptr;
    };

} // namespace Hybrid
