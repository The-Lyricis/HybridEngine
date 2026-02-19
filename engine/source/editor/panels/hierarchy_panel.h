#pragma once
#include "i_editor_panel.h"

namespace Hybrid
{
    class HierarchyPanel final : public IEditorPanel
    {
    public:
        const char* getName() const override { return "Hierarchy"; }
        void onImGuiRender(EditorContext& ctx) override;
    };
}
