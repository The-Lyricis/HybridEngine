#pragma once

#include "editor/editor_ui.h"
#include "editor/engine_services.h"
#include "runtime/core/event/layer.h"

namespace Hybrid
{
    // Editor orchestration layer: UI draw + bridge from EditorContext to render inputs.
    class EditorLayer final : public Layer
    {
    public:
        explicit EditorLayer(EngineServices services);

        void onAttach() override;          // Initialize editor UI and bind scene.
        void onDetach() override;          // Release editor UI resources.
        void onUpdate(float dt) override;  // Pull async pick result from engine.
        void onImGuiRender() override;     // Draw panels and viewport.

    private:
        void updateFrameContext();         // Push current editor state into FrameContext/Flags/Ext.

    private:
        EngineServices m_services{};       // Injected runtime/editor services.
        EditorUI m_editor_ui;              // Panel/UI owner.
        bool m_initialized = false;        // Guard against partial startup/shutdown.
    };
} // namespace Hybrid
