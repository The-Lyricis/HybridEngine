#include "inspector_panel.h"
#include "../editor_context.h"

#include "runtime/function/scene/scene.h"
#include "runtime/function/scene/components.h"

#include <imgui.h>
#include <entt/entt.hpp>
#include <glm/glm.hpp>

namespace Hybrid
{
    static void DrawVec3Control(const char* label, glm::vec3& value, float speed)
    {
        ImGui::PushID(label);
        ImGui::Columns(2);
        ImGui::SetColumnWidth(0, 90.0f);
        ImGui::TextUnformatted(label);
        ImGui::NextColumn();

        ImGui::DragFloat3("##v", &value.x, speed);

        ImGui::Columns(1);
        ImGui::PopID();
    }

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

        auto& reg = ctx.active_scene->getRegistry();

        if (ctx.selected == entt::null || !reg.valid(ctx.selected))
        {
            ImGui::TextDisabled("Nothing selected.");
            ImGui::End();
            return;
        }

        // Name（按 TagComponent 示例）
        if (auto* tag = reg.try_get<TagComponent>(ctx.selected))
        {
            char buffer[256]{};
            strncpy_s(buffer, tag->Tag.c_str(), sizeof(buffer) - 1);

            if (ImGui::InputText("Name", buffer, sizeof(buffer)))
                tag->Tag = buffer;
        }

        ImGui::Separator();

        // Transform
        if (auto* tr = reg.try_get<TransformComponent>(ctx.selected))
        {
            DrawVec3Control("Position", tr->Position, 0.05f);
            DrawVec3Control("Rotation", tr->Rotation, 0.02f);
            DrawVec3Control("Scale", tr->Scale, 0.05f);
        }

        ImGui::End();
    }
}
