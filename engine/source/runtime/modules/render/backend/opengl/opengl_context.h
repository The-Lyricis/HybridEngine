#pragma once

#include "runtime/modules/render/public/graphics_context.h"

namespace Hybrid
{
    class IWindow;

    class GLContext final : public GraphicsContext
    {
    public:
        explicit GLContext(IWindow& window);

        void init() override;
        void swapBuffers() override;

    private:
        IWindow* m_Window = nullptr;
    };
} // namespace Hybrid
