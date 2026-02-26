#pragma once

#include "runtime/core/event/layer.h"

struct GLFWwindow;

namespace Hybrid
{
    // Owns ImGui frame lifecycle (CreateContext/NewFrame/Render/Shutdown).
    class ImGuiLayer final : public Layer
    {
    public:
        explicit ImGuiLayer(GLFWwindow* window);

        void onBeginFrame() override;
        void onAttach() override;
        void onDetach() override;
        void onUpdate(float dt) override;
        void onImGuiRender() override;
        void onEndFrame() override;

    private:
        GLFWwindow* m_window = nullptr; // Host window for ImGui backend.
        bool m_initialized = false;     // Backend/context initialization state.
    };
} // namespace Hybrid
