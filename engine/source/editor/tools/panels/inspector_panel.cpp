#include "inspector_panel.h"
#include "editor/core/component_registry.h"
#include "editor/core/editor_context.h"
#include "editor/core/property_drawer.h"

#include "runtime/modules/scene/scene.h"
#include "runtime/modules/scene/entity.h"
#include "runtime/core/base/math_util.h"
#include "runtime/modules/physics/components/collider_component.h"
#include "runtime/modules/physics/components/rigidbody_component.h"

#include <imgui.h>
#include <entt/entt.hpp>

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
        Entity selected_entity(ctx.selected, &reg, ctx.active_scene);

        if (ctx.selected == entt::null || !reg.valid(ctx.selected))
        {
            ImGui::TextDisabled("Nothing selected.");
            ImGui::End();
            return;
        }

        for (const ComponentDesc& desc : ComponentRegistry::GetDescriptors())
        {
            if (!desc.has || !desc.has(selected_entity))
                continue;

            void* component_ptr = desc.get ? desc.get(selected_entity) : nullptr;
            if (component_ptr == nullptr)
                continue;

            if (!ImGui::CollapsingHeader(desc.name, ImGuiTreeNodeFlags_DefaultOpen))
                continue;

            ImGui::PushID(desc.name);

            if (desc.draw_custom)
            {
                if (desc.draw_custom(ctx, selected_entity, component_ptr))
                    ctx.markSceneDirty();
            }
            else
            {
                for (const PropertyDesc& property : desc.properties)
                {
                    if (DrawPropertyField(component_ptr, property))
                        ctx.markSceneDirty();
                }
            }

            ImGui::PopID();
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
