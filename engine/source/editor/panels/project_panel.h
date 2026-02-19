#pragma once
#include "i_editor_panel.h"

namespace Hybrid
{
    class ProjectPanel final : public IEditorPanel
    {
    public:
        const char* getName() const override { return "Project"; }
        void onImGuiRender(EditorContext& ctx) override;
    };
}
