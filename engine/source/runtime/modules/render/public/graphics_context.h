#pragma once
#include <memory>

namespace Hybrid {

    // GraphicsContext: platform GL/VK context wrapper (make current / swap).
    class GraphicsContext {
    public:
        virtual ~GraphicsContext() = default;

        virtual void init() = 0;          // make context current + load backend
        virtual void swapBuffers() = 0;   // present

        static std::unique_ptr<GraphicsContext> Create(void* nativeWindow);
    };

} // namespace Hybrid
