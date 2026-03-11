#include "component_registry.h"

#include <algorithm>
#include <cstring>

#include <glm/glm.hpp>
#include <imgui.h>

#include "editor/core/editor_context.h"
#include "editor/core/editor_drag_drop.h"
#include "runtime/core/base/math_util.h"
#include "runtime/modules/scene/components.h"
#include "runtime/modules/scene/scene.h"
#include "runtime/modules/scene/components/collider_component.h"
#include "runtime/modules/scene/components/rigidbody_component.h"

namespace Hybrid
{
    namespace
    {
        static const char* kCameraClearModeNames[] =
        {
            "Solid Color",
            "Skybox"
        };

        static const char* kColliderTypeNames[] =
        {
            "None",
            "Box",
            "Sphere"
        };

        template<typename T>
        bool HasComponent(Entity entity)
        {
            return entity && entity.HasComponent<T>();
        }

        template<typename T>
        bool AddComponent(Entity entity)
        {
            if (!entity || entity.HasComponent<T>())
                return false;

            entity.AddComponent<T>();
            return true;
        }

        template<typename T>
        void* GetComponentPtr(Entity entity)
        {
            if (!entity || !entity.HasComponent<T>())
                return nullptr;
            return &entity.GetComponent<T>();
        }

        template<typename T>
        void RemoveComponent(Entity entity)
        {
            if (entity && entity.HasComponent<T>())
                entity.RemoveComponent<T>();
        }

        template<typename T>
        bool* GetEnabledPtr(void* componentPtr)
        {
            if (componentPtr == nullptr)
                return nullptr;
            return &static_cast<T*>(componentPtr)->Enabled;
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
            if (componentPtr == nullptr || ctx.active_scene == nullptr)
                return false;

            auto* tr = static_cast<TransformComponent*>(componentPtr);
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
                ctx.active_scene->MarkDirtyRecursive(entity);
            }

            return transform_changed;
        }

        bool DrawAssetSlot(const char* label, AssetID& assetId)
        {
            bool changed = false;

            ImGui::PushID(label);
            ImGui::Columns(2);
            ImGui::SetColumnWidth(0, 90.0f);
            ImGui::TextUnformatted(label);
            ImGui::NextColumn();

            char buffer[64]{};
            if (assetId.value == 0)
                std::snprintf(buffer, sizeof(buffer), "None");
            else
                std::snprintf(buffer, sizeof(buffer), "%llu", static_cast<unsigned long long>(assetId.value));

            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 64.0f);
            ImGui::InputText("##asset", buffer, sizeof(buffer), ImGuiInputTextFlags_ReadOnly);

            if (ImGui::BeginDragDropTarget())
            {
                AssetID dropped{};
                if (EditorDragDrop::AcceptAsset(dropped))
                {
                    assetId = dropped;
                    changed = true;
                }
                ImGui::EndDragDropTarget();
            }

            ImGui::SameLine();
            if (ImGui::SmallButton("Clear"))
            {
                if (assetId.value != 0)
                {
                    assetId = AssetID{};
                    changed = true;
                }
            }

            ImGui::Columns(1);
            ImGui::PopID();
            return changed;
        }

        bool DrawCameraComponent(EditorContext&, Entity, void* componentPtr)
        {
            if (componentPtr == nullptr)
                return false;

            auto* camera = static_cast<CameraComponent*>(componentPtr);
            bool changed = false;

            changed |= ImGui::Checkbox("Primary", &camera->Primary);
            changed |= ImGui::SliderFloat("FovY", &camera->FovY, 1.0f, 179.0f);
            changed |= ImGui::SliderFloat("Near", &camera->Near, 0.001f, 100.0f);
            changed |= ImGui::SliderFloat("Far", &camera->Far, 1.0f, 10000.0f);

            int clear_mode = static_cast<int>(camera->ClearMode);
            if (ImGui::Combo("Clear Mode", &clear_mode, kCameraClearModeNames, IM_ARRAYSIZE(kCameraClearModeNames)))
            {
                camera->ClearMode = static_cast<CameraClearMode>(clear_mode);
                changed = true;
            }

            if (camera->ClearMode == CameraClearMode::SolidColor)
                changed |= ImGui::ColorEdit4("Clear Color", &camera->ClearColor.x);

            return changed;
        }

        bool DrawMeshRendererComponent(EditorContext&, Entity, void* componentPtr)
        {
            if (componentPtr == nullptr)
                return false;

            auto* mr = static_cast<MeshRendererComponent*>(componentPtr);
            bool changed = false;

            changed |= DrawAssetSlot("Mesh", mr->Mesh);
            changed |= DrawAssetSlot("Material", mr->Material);

            ImGui::PushID("Tint");
            ImGui::Columns(2);
            ImGui::SetColumnWidth(0, 90.0f);
            ImGui::TextUnformatted("Tint");
            ImGui::NextColumn();
            changed |= ImGui::ColorEdit4("##tint", &mr->Tint.x);
            ImGui::Columns(1);
            ImGui::PopID();

            return changed;
        }

        bool DrawColliderComponent(EditorContext& ctx, Entity entity, void* componentPtr)
        {
            if (componentPtr == nullptr)
                return false;

            auto* collider = static_cast<ColliderComponent*>(componentPtr);
            bool changed = false;

            changed |= ImGui::Checkbox("Is Trigger", &collider->IsTrigger);

            int type_index = static_cast<int>(collider->Type);
            if (ImGui::Combo("Type", &type_index, kColliderTypeNames, IM_ARRAYSIZE(kColliderTypeNames)))
            {
                collider->Type = static_cast<ColliderType>(type_index);
                changed = true;
            }

            changed |= DrawVec3Control("Center", collider->Center, 0.05f);

            switch (collider->Type)
            {
            case ColliderType::Box:
                changed |= DrawVec3Control("Half Extents", collider->Box.HalfExtents, 0.05f);
                collider->Box.HalfExtents.x = std::max(0.0f, collider->Box.HalfExtents.x);
                collider->Box.HalfExtents.y = std::max(0.0f, collider->Box.HalfExtents.y);
                collider->Box.HalfExtents.z = std::max(0.0f, collider->Box.HalfExtents.z);
                break;
            case ColliderType::Sphere:
                ImGui::PushID("Radius");
                ImGui::Columns(2);
                ImGui::SetColumnWidth(0, 90.0f);
                ImGui::TextUnformatted("Radius");
                ImGui::NextColumn();
                if (ImGui::DragFloat("##v", &collider->Sphere.Radius, 0.05f, 0.0f, 1000.0f))
                    changed = true;
                ImGui::Columns(1);
                ImGui::PopID();

                collider->Sphere.Radius = std::max(0.0f, collider->Sphere.Radius);
                break;
            default:
                break;
            }

            const bool can_fit =
                collider->Type == ColliderType::Box &&
                entity.HasComponent<MeshRendererComponent>() &&
                entity.GetComponent<MeshRendererComponent>().Mesh.value != 0 &&
                static_cast<bool>(ctx.fit_box_collider_to_mesh);

            if (!can_fit)
                ImGui::BeginDisabled();
            if (ImGui::Button("Fit To Mesh"))
            {
                if (ctx.fit_box_collider_to_mesh && ctx.fit_box_collider_to_mesh(entity.GetHandle()))
                    changed = true;
            }
            if (!can_fit)
                ImGui::EndDisabled();

            return changed;
        }

        std::vector<ComponentDesc> BuildDescriptors()
        {
            std::vector<ComponentDesc> descriptors;

            ComponentDesc tag_desc;
            tag_desc.name = "Tag";
            tag_desc.flags = ComponentFlags::Serializable;
            tag_desc.has = &HasComponent<TagComponent>;
            tag_desc.add = &AddComponent<TagComponent>;
            tag_desc.get = &GetComponentPtr<TagComponent>;
            tag_desc.remove = &RemoveComponent<TagComponent>;
            tag_desc.properties = {
                PropertyDesc{
                    "Name",
                    PropertyType::String,
                    offsetof(TagComponent, Tag),
                    0.1f,
                    0.0f,
                    0.0f,
                    false,
                    nullptr,
                    PropertyFlags::Visible | PropertyFlags::Editable | PropertyFlags::Serializable,
                    nullptr
                }
            };
            descriptors.push_back(tag_desc);

            ComponentDesc transform_desc;
            transform_desc.name = "Transform";
            transform_desc.flags = ComponentFlags::Serializable;
            transform_desc.has = &HasComponent<TransformComponent>;
            transform_desc.add = &AddComponent<TransformComponent>;
            transform_desc.get = &GetComponentPtr<TransformComponent>;
            transform_desc.remove = &RemoveComponent<TransformComponent>;
            transform_desc.draw_custom = &DrawTransformComponent;
            descriptors.push_back(transform_desc);

            ComponentDesc camera_desc;
            camera_desc.name = "Camera";
            camera_desc.flags = ComponentFlags::Serializable | ComponentFlags::Addable | ComponentFlags::Removable;
            camera_desc.has = &HasComponent<CameraComponent>;
            camera_desc.add = &AddComponent<CameraComponent>;
            camera_desc.get = &GetComponentPtr<CameraComponent>;
            camera_desc.remove = &RemoveComponent<CameraComponent>;
            camera_desc.enabled = &GetEnabledPtr<CameraComponent>;
            camera_desc.draw_custom = &DrawCameraComponent;
            descriptors.push_back(camera_desc);

            ComponentDesc directional_light_desc;
            directional_light_desc.name = "Directional Light";
            directional_light_desc.flags = ComponentFlags::Serializable | ComponentFlags::Addable | ComponentFlags::Removable;
            directional_light_desc.has = &HasComponent<DirectionalLightComponent>;
            directional_light_desc.add = &AddComponent<DirectionalLightComponent>;
            directional_light_desc.get = &GetComponentPtr<DirectionalLightComponent>;
            directional_light_desc.remove = &RemoveComponent<DirectionalLightComponent>;
            directional_light_desc.enabled = &GetEnabledPtr<DirectionalLightComponent>;
            directional_light_desc.properties = {
                PropertyDesc{
                    "Color",
                    PropertyType::Vec3,
                    offsetof(DirectionalLightComponent, Color),
                    0.1f,
                    0.0f,
                    0.0f,
                    false,
                    "Light color.",
                    PropertyFlags::Visible | PropertyFlags::Editable | PropertyFlags::Serializable | PropertyFlags::Color
                },
                PropertyDesc{
                    "Intensity",
                    PropertyType::Float,
                    offsetof(DirectionalLightComponent, Intensity),
                    0.05f,
                    0.0f,
                    100.0f,
                    true,
                    "Directional light intensity."
                }
            };
            descriptors.push_back(directional_light_desc);

            ComponentDesc point_light_desc;
            point_light_desc.name = "Point Light";
            point_light_desc.flags = ComponentFlags::Serializable | ComponentFlags::Addable | ComponentFlags::Removable;
            point_light_desc.has = &HasComponent<PointLightComponent>;
            point_light_desc.add = &AddComponent<PointLightComponent>;
            point_light_desc.get = &GetComponentPtr<PointLightComponent>;
            point_light_desc.remove = &RemoveComponent<PointLightComponent>;
            point_light_desc.enabled = &GetEnabledPtr<PointLightComponent>;
            point_light_desc.properties = {
                PropertyDesc{
                    "Color",
                    PropertyType::Vec3,
                    offsetof(PointLightComponent, Color),
                    0.1f,
                    0.0f,
                    0.0f,
                    false,
                    "Light color.",
                    PropertyFlags::Visible | PropertyFlags::Editable | PropertyFlags::Serializable | PropertyFlags::Color
                },
                PropertyDesc{
                    "Intensity",
                    PropertyType::Float,
                    offsetof(PointLightComponent, Intensity),
                    0.05f,
                    0.0f,
                    100.0f,
                    true,
                    "Point light intensity."
                },
                PropertyDesc{
                    "Range",
                    PropertyType::Float,
                    offsetof(PointLightComponent, Range),
                    0.1f,
                    0.0f,
                    1000.0f,
                    true,
                    "Point light attenuation range."
                }
            };
            descriptors.push_back(point_light_desc);

            ComponentDesc collider_desc;
            collider_desc.name = "Collider";
            collider_desc.flags = ComponentFlags::Serializable | ComponentFlags::Addable | ComponentFlags::Removable;
            collider_desc.has = &HasComponent<ColliderComponent>;
            collider_desc.add = &AddComponent<ColliderComponent>;
            collider_desc.get = &GetComponentPtr<ColliderComponent>;
            collider_desc.remove = &RemoveComponent<ColliderComponent>;
            collider_desc.enabled = &GetEnabledPtr<ColliderComponent>;
            collider_desc.draw_custom = &DrawColliderComponent;
            descriptors.push_back(collider_desc);

            ComponentDesc rigidbody_desc;
            rigidbody_desc.name = "Rigidbody";
            rigidbody_desc.flags = ComponentFlags::Serializable | ComponentFlags::Addable | ComponentFlags::Removable;
            rigidbody_desc.has = &HasComponent<RigidbodyComponent>;
            rigidbody_desc.add = &AddComponent<RigidbodyComponent>;
            rigidbody_desc.get = &GetComponentPtr<RigidbodyComponent>;
            rigidbody_desc.remove = &RemoveComponent<RigidbodyComponent>;
            rigidbody_desc.enabled = &GetEnabledPtr<RigidbodyComponent>;
            rigidbody_desc.properties = {
                PropertyDesc{
                    "Velocity",
                    PropertyType::Vec3,
                    offsetof(RigidbodyComponent, Velocity),
                    0.05f,
                    0.0f,
                    0.0f,
                    false,
                    "Linear velocity in world space."
                },
                PropertyDesc{
                    "Force",
                    PropertyType::Vec3,
                    offsetof(RigidbodyComponent, Force),
                    0.05f,
                    0.0f,
                    0.0f,
                    false,
                    "Accumulated force in world space."
                },
                PropertyDesc{
                    "Mass",
                    PropertyType::Float,
                    offsetof(RigidbodyComponent, Mass),
                    0.05f,
                    0.001f,
                    1000.0f,
                    true,
                    "Rigid body mass."
                },
                PropertyDesc{
                    "UseGravity",
                    PropertyType::Bool,
                    offsetof(RigidbodyComponent, UseGravity),
                    0.1f,
                    0.0f,
                    0.0f,
                    false,
                    "Whether gravity affects this body."
                },
                PropertyDesc{
                    "IsKinematic",
                    PropertyType::Bool,
                    offsetof(RigidbodyComponent, IsKinematic),
                    0.1f,
                    0.0f,
                    0.0f,
                    false,
                    "Whether the rigid body is driven externally."
                }
            };
            descriptors.push_back(rigidbody_desc);

            ComponentDesc mesh_renderer_desc;
            mesh_renderer_desc.name = "Mesh Renderer";
            mesh_renderer_desc.flags = ComponentFlags::Serializable | ComponentFlags::Addable | ComponentFlags::Removable;
            mesh_renderer_desc.has = &HasComponent<MeshRendererComponent>;
            mesh_renderer_desc.add = &AddComponent<MeshRendererComponent>;
            mesh_renderer_desc.get = &GetComponentPtr<MeshRendererComponent>;
            mesh_renderer_desc.remove = &RemoveComponent<MeshRendererComponent>;
            mesh_renderer_desc.enabled = &GetEnabledPtr<MeshRendererComponent>;
            mesh_renderer_desc.draw_custom = &DrawMeshRendererComponent;
            descriptors.push_back(mesh_renderer_desc);

            return descriptors;
        }
    }

    const std::vector<ComponentDesc>& ComponentRegistry::GetDescriptors()
    {
        static const std::vector<ComponentDesc> s_descriptors = BuildDescriptors();
        return s_descriptors;
    }
} // namespace Hybrid
