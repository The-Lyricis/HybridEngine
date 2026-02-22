#include "project_panel.h"
#include "../editor_context.h"
#include <imgui.h>

namespace Hybrid
{
    void ProjectPanel::onImGuiRender(EditorContext&)
    {
        if (!m_open) return;

        ImGui::Begin(getName(), &m_open);
        ImGui::Text("File Explorer and project settings..");
        ImGui::End();
    }
}
