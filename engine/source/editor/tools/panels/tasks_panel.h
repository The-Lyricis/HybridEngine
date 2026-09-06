#pragma once

#include "i_editor_panel.h"

namespace Hybrid
{
    class TasksPanel final : public IEditorPanel
    {
    public:
        TasksPanel() : IEditorPanel(EditorPanelId::Tasks, "Tasks") {}
        void onImGuiRender(EditorContext& ctx) override;
    };
}
