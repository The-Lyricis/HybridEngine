#include "hierarchy_panel.h"
#include "../editor_context.h"
#include <imgui.h>

namespace Hybrid
{
    void HierarchyPanel::onImGuiRender(EditorContext& ctx)
    {
        if (!m_open) return;

        ImGui::Begin(getName(), &m_open);

        if (!ctx.active_scene)
        {
            ImGui::TextDisabled("No active scene.");
            ImGui::End();
            return;
        }

        // 第一步：占位
        ImGui::Text("Hierarchy (step1 skeleton)");
        ImGui::Separator();
        ImGui::Text("Selected ID: %llu", (unsigned long long)ctx.selected_id);

        // TODO(step2): 遍历场景实体，点击时写 ctx.selected_id

        ImGui::End();
    }
}
