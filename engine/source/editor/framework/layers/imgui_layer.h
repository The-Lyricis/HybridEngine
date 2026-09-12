#pragma once

#include <memory>

#include "runtime/core/event/layer.h"
#include "runtime/core/platform/window.h"

namespace Hybrid
{
    class IImGuiRenderBackend;

    // Owns ImGui frame lifecycle (CreateContext/NewFrame/Render/Shutdown).
    class ImGuiLayer final : public Layer
    {
    public:
        ImGuiLayer(IWindow& window, std::shared_ptr<IImGuiRenderBackend> backend);
        ~ImGuiLayer() override;

        void onBeginFrame() override;
        void onAttach() override;
        void onDetach() override;
        void onUpdate(float dt) override;
        void onImGuiRender() override;
        void onEndFrame() override;

    private:
        IWindow* m_window = nullptr;
        std::shared_ptr<IImGuiRenderBackend> m_backend;
        bool m_initialized = false;     // Backend/context initialization state.
    };
} // namespace Hybrid
