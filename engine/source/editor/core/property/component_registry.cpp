#include "component_registry.h"

#include <algorithm>
#include <cstring>
#include <string>
#include <unordered_map>

#include <glm/glm.hpp>
#include <imgui.h>

#include "editor/core/commands/component_value_command.h"
#include "editor/core/context/editor_context.h"
#include "editor/core/property/property_drawer.h"
#include "runtime/core/base/math_util.h"
#include "runtime/modules/scene/components.h"
#include "runtime/modules/scene/scene.h"
#include "runtime/modules/scene/components/collider_component.h"
#include "runtime/modules/scene/components/rigidbody_component.h"

namespace Hybrid
{
    namespace
    {
        glm::vec3 NormalizeEulerDegrees(glm::vec3 euler_deg)
        {
            if (euler_deg.x > 180.0f) euler_deg.x -= 360.0f;
            if (euler_deg.y > 180.0f) euler_deg.y -= 360.0f;
            if (euler_deg.z > 180.0f) euler_deg.z -= 360.0f;
            if (euler_deg.x < -180.0f) euler_deg.x += 360.0f;
            if (euler_deg.y < -180.0f) euler_deg.y += 360.0f;
            if (euler_deg.z < -180.0f) euler_deg.z += 360.0f;
            return euler_deg;
        }

        bool DrawVec3Control(const char* label, glm::vec3& value, float speed)
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

        bool DrawTransformComponent(EditorContext& ctx, Entity entity, void* componentPtr)
        {
            if (componentPtr == nullptr || ctx.document.activeScene() == nullptr)
                return false;

            auto* tr = static_cast<TransformComponent*>(componentPtr);
            bool transform_changed = false;

            transform_changed |= DrawTrackedContinuousComponentEdit(ctx,
                                                                    entity,
                                                                    *tr,
                                                                    "Set Transform Position",
                                                                    [&]()
                                                                    {
                                                                        return DrawVec3Control("Position", tr->Position, 0.05f);
                                                                    });

            ImGui::PushID("Rotation");
            ImGui::Columns(2);
            ImGui::SetColumnWidth(0, 90.0f);
            ImGui::TextUnformatted("Rotation");
            ImGui::NextColumn();

            const uint32_t entity_id = static_cast<uint32_t>(entt::to_integral(entity.GetHandle()));
            const size_t command_hash = std::hash<std::string_view>{}(std::string_view("Set Transform Rotation"));
            const ImGuiID rotation_session_id =
                static_cast<ImGuiID>(command_hash ^ (static_cast<size_t>(entity_id) * 16777619ull));

            static std::unordered_map<ImGuiID, glm::vec3> s_rotation_sessions;
            glm::vec3 euler_deg = NormalizeEulerDegrees(MathUtil::eulerDegreesFromQuat(tr->Rotation));
            if (const auto it = s_rotation_sessions.find(rotation_session_id); it != s_rotation_sessions.end())
                euler_deg = it->second;

            const bool rot_changed = DrawTrackedContinuousComponentEdit(ctx,
                                                                        entity,
                                                                        *tr,
                                                                        "Set Transform Rotation",
                                                                        [&]()
                                                                        {
                                                                            return ImGui::DragFloat3("##v", &euler_deg.x, 0.2f);
                                                                        },
                                                                        [&](TransformComponent& component)
                                                                        {
                                                                            component.Rotation = MathUtil::quatFromEulerDegrees(euler_deg);
                                                                        });

            if (ImGui::IsItemActivated() || ImGui::IsItemActive() || rot_changed)
                s_rotation_sessions[rotation_session_id] = euler_deg;

            const bool rotation_edit_ended =
                ImGui::IsItemDeactivatedAfterEdit() ||
                (!ImGui::IsItemActive() && !ImGui::IsMouseDown(ImGuiMouseButton_Left));
            if (rotation_edit_ended)
                s_rotation_sessions.erase(rotation_session_id);

            ImGui::Columns(1);
            ImGui::PopID();
            transform_changed |= rot_changed;

            transform_changed |= DrawTrackedContinuousComponentEdit(ctx,
                                                                    entity,
                                                                    *tr,
                                                                    "Set Transform Scale",
                                                                    [&]()
                                                                    {
                                                                        return DrawVec3Control("Scale", tr->Scale, 0.05f);
                                                                    });

            if (transform_changed)
            {
                tr->DirtyLocal = true;
                ctx.document.activeScene()->MarkDirtyRecursive(entity);
            }

            return transform_changed;
        }

        bool DrawCameraComponent(EditorContext& ctx, Entity entity, void* componentPtr)
        {
            if (componentPtr == nullptr)
                return false;

            auto* camera = static_cast<CameraComponent*>(componentPtr);
            bool changed = false;

            if (const ComponentSchema* schema = FindComponentSchema(SceneComponentType::Camera))
            {
                for (const PropertyDesc& property : schema->properties)
                {
                    if (std::strcmp(property.name, "clearColor") == 0 &&
                        camera->ClearMode != CameraClearMode::SolidColor)
                    {
                        continue;
                    }

                    changed |= DrawTrackedPropertyField(ctx, entity, *camera, property);
                }
            }

            return changed;
        }

        bool DrawMeshRendererComponent(EditorContext& ctx, Entity entity, void* componentPtr)
        {
            if (componentPtr == nullptr)
                return false;

            auto* mr = static_cast<MeshRendererComponent*>(componentPtr);
            bool changed = false;

            if (const ComponentSchema* schema = FindComponentSchema(SceneComponentType::MeshRenderer))
            {
                for (const PropertyDesc& property : schema->properties)
                    changed |= DrawTrackedPropertyField(ctx, entity, *mr, property);
            }

            return changed;
        }

        bool DrawColliderComponent(EditorContext& ctx, Entity entity, void* componentPtr)
        {
            if (componentPtr == nullptr)
                return false;

            auto* collider = static_cast<ColliderComponent*>(componentPtr);
            bool changed = false;

            if (const ComponentSchema* schema = FindComponentSchema(SceneComponentType::Collider))
            {
                for (const PropertyDesc& property : schema->properties)
                    changed |= DrawTrackedPropertyField(ctx, entity, *collider, property);
            }

            switch (collider->Type)
            {
            case ColliderType::Box:
                changed |= DrawTrackedContinuousComponentEdit(ctx,
                                                              entity,
                                                              *collider,
                                                              "Set Box Collider Half Extents",
                                                              [&]() { return DrawVec3Control("Half Extents", collider->Box.HalfExtents, 0.05f); },
                                                              [&](ColliderComponent& component)
                                                              {
                                                                  component.Box.HalfExtents.x = std::max(0.0f, component.Box.HalfExtents.x);
                                                                  component.Box.HalfExtents.y = std::max(0.0f, component.Box.HalfExtents.y);
                                                                  component.Box.HalfExtents.z = std::max(0.0f, component.Box.HalfExtents.z);
                                                              });
                break;
            case ColliderType::Sphere:
                ImGui::PushID("Radius");
                ImGui::Columns(2);
                ImGui::SetColumnWidth(0, 90.0f);
                ImGui::TextUnformatted("Radius");
                ImGui::NextColumn();
                changed |= DrawTrackedContinuousComponentEdit(ctx,
                                                              entity,
                                                              *collider,
                                                              "Set Sphere Collider Radius",
                                                              [&]() { return ImGui::DragFloat("##v", &collider->Sphere.Radius, 0.05f, 0.0f, 1000.0f); },
                                                              [&](ColliderComponent& component)
                                                              {
                                                                  component.Sphere.Radius = std::max(0.0f, component.Sphere.Radius);
                                                              });
                ImGui::Columns(1);
                ImGui::PopID();
                break;
            default:
                break;
            }

            const bool can_fit =
                collider->Type == ColliderType::Box &&
                entity.HasComponent<MeshRendererComponent>() &&
                entity.GetComponent<MeshRendererComponent>().Mesh.value != 0 &&
                static_cast<bool>(ctx.scene_actions.fit_box_collider_to_mesh);

            if (!can_fit)
                ImGui::BeginDisabled();
            if (ImGui::Button("Fit To Mesh"))
            {
                if (ctx.scene_actions.fit_box_collider_to_mesh)
                {
                    const ColliderComponent before = *collider;
                    if (ctx.scene_actions.fit_box_collider_to_mesh(entity.GetHandle()))
                    {
                        CommitComponentValueChange(ctx,
                                                   entity,
                                                   "Fit Box Collider To Mesh",
                                                   before,
                                                   *collider);
                        changed = true;
                    }
                }
            }
            if (!can_fit)
                ImGui::EndDisabled();

            return changed;
        }

        bool DrawDirectionalLightComponent(EditorContext& ctx, Entity entity, void* componentPtr)
        {
            if (componentPtr == nullptr)
                return false;

            auto* light = static_cast<DirectionalLightComponent*>(componentPtr);
            bool changed = false;
            changed |= DrawTrackedContinuousComponentEdit(ctx,
                                                          entity,
                                                          *light,
                                                          "Set Directional Light Color",
                                                          [&]() { return ImGui::ColorEdit3("Color", &light->Color.x); });
            changed |= DrawTrackedContinuousComponentEdit(ctx,
                                                          entity,
                                                          *light,
                                                          "Set Directional Light Intensity",
                                                          [&]() { return ImGui::DragFloat("Intensity", &light->Intensity, 0.05f, 0.0f, 100.0f); });
            return changed;
        }

        bool DrawPointLightComponent(EditorContext& ctx, Entity entity, void* componentPtr)
        {
            if (componentPtr == nullptr)
                return false;

            auto* light = static_cast<PointLightComponent*>(componentPtr);
            bool changed = false;
            changed |= DrawTrackedContinuousComponentEdit(ctx,
                                                          entity,
                                                          *light,
                                                          "Set Point Light Color",
                                                          [&]() { return ImGui::ColorEdit3("Color", &light->Color.x); });
            changed |= DrawTrackedContinuousComponentEdit(ctx,
                                                          entity,
                                                          *light,
                                                          "Set Point Light Intensity",
                                                          [&]() { return ImGui::DragFloat("Intensity", &light->Intensity, 0.05f, 0.0f, 100.0f); });
            changed |= DrawTrackedContinuousComponentEdit(ctx,
                                                          entity,
                                                          *light,
                                                          "Set Point Light Range",
                                                          [&]() { return ImGui::DragFloat("Range", &light->Range, 0.1f, 0.0f, 1000.0f); },
                                                          [&](PointLightComponent& component)
                                                          {
                                                              component.Range = std::max(0.0f, component.Range);
                                                          });
            return changed;
        }

        bool DrawRigidbodyComponent(EditorContext& ctx, Entity entity, void* componentPtr)
        {
            if (componentPtr == nullptr)
                return false;

            auto* rigidbody = static_cast<RigidbodyComponent*>(componentPtr);
            bool changed = false;
            changed |= DrawTrackedContinuousComponentEdit(ctx,
                                                          entity,
                                                          *rigidbody,
                                                          "Set Rigidbody Velocity",
                                                          [&]() { return DrawVec3Control("Velocity", rigidbody->Velocity, 0.05f); });
            changed |= DrawTrackedContinuousComponentEdit(ctx,
                                                          entity,
                                                          *rigidbody,
                                                          "Set Rigidbody Constant Force",
                                                          [&]() { return DrawVec3Control("Constant Force", rigidbody->ConstantForce, 0.05f); });
            changed |= DrawTrackedContinuousComponentEdit(ctx,
                                                          entity,
                                                          *rigidbody,
                                                          "Set Rigidbody Mass",
                                                          [&]() { return ImGui::DragFloat("Mass", &rigidbody->Mass, 0.05f, 0.001f, 1000.0f); },
                                                          [&](RigidbodyComponent& component)
                                                          {
                                                              component.Mass = std::max(0.001f, component.Mass);
                                                          });
            changed |= DrawTrackedImmediateComponentEdit(ctx,
                                                         entity,
                                                         *rigidbody,
                                                         "Set Rigidbody Gravity",
                                                         [&]() { return ImGui::Checkbox("UseGravity", &rigidbody->UseGravity); });
            changed |= DrawTrackedImmediateComponentEdit(ctx,
                                                         entity,
                                                         *rigidbody,
                                                         "Set Rigidbody Kinematic",
                                                         [&]() { return ImGui::Checkbox("IsKinematic", &rigidbody->IsKinematic); });
            return changed;
        }

        ComponentCustomDrawFn ResolveCustomDraw(SceneComponentType type)
        {
            switch (type)
            {
            case SceneComponentType::Tag: return nullptr;
            case SceneComponentType::Transform: return &DrawTransformComponent;
            case SceneComponentType::Camera: return &DrawCameraComponent;
            case SceneComponentType::MeshRenderer: return nullptr;
            case SceneComponentType::DirectionalLight: return nullptr;
            case SceneComponentType::PointLight: return nullptr;
            case SceneComponentType::Collider: return &DrawColliderComponent;
            case SceneComponentType::Rigidbody: return nullptr;
            default: return nullptr;
            }
        }

        std::vector<ComponentDesc> BuildDescriptors()
        {
            std::vector<ComponentDesc> descriptors;
            const std::vector<ComponentSchema>& schemas = GetComponentSchemas();
            descriptors.reserve(schemas.size());

            for (const ComponentSchema& schema : schemas)
            {
                ComponentDesc desc{};
                desc.schema = &schema;
                desc.draw_custom = ResolveCustomDraw(schema.type);
                desc.name = schema.name;
                desc.flags = schema.flags;
                desc.has = schema.has;
                desc.add = schema.add;
                desc.get = schema.get;
                desc.remove = schema.remove;
                desc.enabled = schema.enabled;
                desc.properties = schema.properties;
                descriptors.push_back(std::move(desc));
            }

            return descriptors;
        }
    }

    const std::vector<ComponentDesc>& ComponentRegistry::GetDescriptors()
    {
        static const std::vector<ComponentDesc> s_descriptors = BuildDescriptors();
        return s_descriptors;
    }
} // namespace Hybrid
