#include "inspector_panel.h"
#include "editor/core/editor_context.h"

#include "runtime/function/scene/scene.h"
#include "runtime/function/scene/components.h"
#include "runtime/function/scene/entity.h"
#include "runtime/core/base/math_util.h"

#include <imgui.h>
#include <entt/entt.hpp>
#include <glm/glm.hpp>

namespace Hybrid
{
    static bool DrawVec3Control(const char* label, glm::vec3& value, float speed)
    {
        ImGui::PushID(label);
        ImGui::Columns(2);
        ImGui::SetColumnWidth(0, 90.0f);
        ImGui::TextUnformatted(label);
        ImGui::NextColumn();

        const bool changed = ImGui::DragFloat3("##v", &value.x, speed);

        ImGui::Columns(1);
        ImGui::PopID();
        return changed;
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

        // Name锛堟寜 TagComponent 绀轰緥锛?
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
            bool transform_changed = false;
            transform_changed |= DrawVec3Control("Position", tr->Position, 0.05f);
            glm::vec3 euler_deg = MathUtil::eulerDegreesFromQuat(tr->Rotation);
            if (euler_deg.x > 180.0f) euler_deg.x -= 360.0f;
            if (euler_deg.y > 180.0f) euler_deg.y -= 360.0f;
            if (euler_deg.z > 180.0f) euler_deg.z -= 360.0f;
            if (euler_deg.x < -180.0f) euler_deg.x += 360.0f;
            if (euler_deg.y < -180.0f) euler_deg.y += 360.0f;
            if (euler_deg.z < -180.0f) euler_deg.z += 360.0f;

            ImGui::PushID("Rotation");
            ImGui::Columns(2);
            ImGui::SetColumnWidth(0, 90.0f);
            ImGui::TextUnformatted("Rotation");
            ImGui::NextColumn();
            const bool rot_changed = ImGui::DragFloat3("##v", &euler_deg.x, 0.2f);
            ImGui::Columns(1);
            ImGui::PopID();

            if (rot_changed)
            {
                tr->Rotation = MathUtil::quatFromEulerDegrees(euler_deg);
                transform_changed = true;
            }
            transform_changed |= DrawVec3Control("Scale", tr->Scale, 0.05f);

            if (transform_changed)
            {
                tr->DirtyLocal = true;
                ctx.active_scene->MarkDirtyRecursive(Entity(ctx.selected, &reg, ctx.active_scene));
            }
        }

        ImGui::End();
    }
}

