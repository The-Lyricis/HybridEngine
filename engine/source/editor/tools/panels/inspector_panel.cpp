#include "inspector_panel.h"
#include "editor/core/editor_context.h"

#include "runtime/modules/scene/scene.h"
#include "runtime/modules/scene/components.h"
#include "runtime/modules/scene/entity.h"
#include "runtime/core/base/math_util.h"
#include "runtime/modules/physics/components/collider_component.h"
#include "runtime/modules/physics/components/rigidbody_component.h"

#include <imgui.h>
#include <entt/entt.hpp>
#include <glm/glm.hpp>

namespace Hybrid
{
    static const char* kColliderTypeNames[] =
    {
        "None",
        "Box",
        "Sphere"
    };
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

        // Name (from TagComponent).
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
        ImGui::Separator();

        // Collider
        if (auto* collider = reg.try_get<ColliderComponent>(ctx.selected))
        {
            if (ImGui::TreeNodeEx("Collider", ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::Checkbox("Enabled", &collider->Enabled);
                ImGui::Checkbox("Is Trigger", &collider->IsTrigger);

                int type_index = static_cast<int>(collider->Type);
                if (ImGui::Combo("Type", &type_index, kColliderTypeNames, IM_ARRAYSIZE(kColliderTypeNames)))
                {
                    collider->Type = static_cast<ColliderType>(type_index);
                }

                DrawVec3Control("Center", collider->Center, 0.05f);

                switch (collider->Type)
                {
                case ColliderType::Box:
                {
                    DrawVec3Control("Half Extents", collider->Box.HalfExtents, 0.05f);

                    collider->Box.HalfExtents.x = std::max(0.0f, collider->Box.HalfExtents.x);
                    collider->Box.HalfExtents.y = std::max(0.0f, collider->Box.HalfExtents.y);
                    collider->Box.HalfExtents.z = std::max(0.0f, collider->Box.HalfExtents.z);
                    break;
                }
                case ColliderType::Sphere:
                {
                    ImGui::PushID("Radius");
                    ImGui::Columns(2);
                    ImGui::SetColumnWidth(0, 90.0f);
                    ImGui::TextUnformatted("Radius");
                    ImGui::NextColumn();

                    ImGui::DragFloat("##v", &collider->Sphere.Radius, 0.05f, 0.0f, 1000.0f);

                    ImGui::Columns(1);
                    ImGui::PopID();

                    collider->Sphere.Radius = std::max(0.0f, collider->Sphere.Radius);
                    break;
                }
                default:
                    break;
                }

                ImGui::TreePop();
            }
        }
        ImGui::Separator();

        // Rigidbody
        if (auto* rigidbody = reg.try_get<RigidbodyComponent>(ctx.selected))
        {
            if (ImGui::TreeNodeEx("Rigidbody", ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::Checkbox("Use Gravity", &rigidbody->UseGravity);
                ImGui::Checkbox("Is Kinematic", &rigidbody->IsKinematic);

                ImGui::PushID("Mass");
                ImGui::Columns(2);
                ImGui::SetColumnWidth(0, 90.0f);
                ImGui::TextUnformatted("Mass");
                ImGui::NextColumn();
                ImGui::DragFloat("##v", &rigidbody->Mass, 0.05f, 0.0f, 1000.0f);
                ImGui::Columns(1);
                ImGui::PopID();

                rigidbody->Mass = std::max(0.0f, rigidbody->Mass);

                DrawVec3Control("Velocity", rigidbody->Velocity, 0.05f);
                DrawVec3Control("Force", rigidbody->Force, 0.05f);

                ImGui::TreePop();
            }
        }

        ImGui::End();
    }
}
