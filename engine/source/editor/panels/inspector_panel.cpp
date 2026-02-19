#include "inspector_panel.h"
#include "../editor_context.h"
#include <imgui.h>

namespace Hybrid
{
    void InspectorPanel::onImGuiRender(EditorContext& ctx)
    {
        if (!m_open) return;

        ImGui::Begin(getName(), &m_open);

        if (!ctx.active_scene)
        {
            ImGui::TextDisabled("No active scene.");
            ImGui::End();
            return;
        }

        if (ctx.selected_id == 0)
        {
            ImGui::TextDisabled("Nothing selected.");
            ImGui::End();
            return;
        }

        // 第一步：占位
        ImGui::Text("Inspector (step1 skeleton)");
        ImGui::Separator();
        ImGui::Text("Selected ID: %llu", (unsigned long long)ctx.selected_id);

        // TODO(step2): 读取 selected 的组件并绘制可编辑项（Transform 等）

        ImGui::End();
    }
}
