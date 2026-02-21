#pragma once

#include <memory>

#include <glm/vec2.hpp>

namespace Hybrid
{
    class Scene;
    class InputState;

    // Runtime-visible frame payload used by RenderSystem.
    struct FrameContext
    {
        std::shared_ptr<Scene> scene;             // Active scene for extraction.
        glm::vec2 viewport_size{0.0f, 0.0f};      // Render target size in pixels.
        void* window_handle = nullptr;            // Native window handle for backend calls.
        float dt = 0.0f;                          // Frame delta time in seconds.
        const InputState* input = nullptr;        // Input snapshot for editor camera control.
    };
} // namespace Hybrid
