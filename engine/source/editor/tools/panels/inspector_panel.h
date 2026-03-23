#pragma once
#include "i_editor_panel.h"

namespace Hybrid
{
    class InspectorPanel final : public IEditorPanel
    {
    public:
        InspectorPanel() : IEditorPanel(EditorPanelId::Inspector, "Inspector") {}
        void onImGuiRender(EditorContext& ctx) override;
    };
}
