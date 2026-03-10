#include "component_registry.h"

#include <cstring>

#include <glm/glm.hpp>
#include <imgui.h>

#include "editor/core/editor_context.h"
#include "editor/core/editor_drag_drop.h"
#include "runtime/core/base/math_util.h"
#include "runtime/modules/scene/components.h"
#include "runtime/modules/scene/scene.h"
#include "runtime/modules/scene/components/rigidbody_component.h"

namespace Hybrid
{
    namespace
    {
        template<typename T>
        bool HasComponent(Entity entity)
        {
            return entity && entity.HasComponent<T>();
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

        std::vector<ComponentDesc> BuildDescriptors()
        {
            std::vector<ComponentDesc> descriptors;

            ComponentDesc tag_desc;
            tag_desc.name = "Tag";
            tag_desc.flags = ComponentFlags::Serializable;
            tag_desc.has = &HasComponent<TagComponent>;
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
            transform_desc.get = &GetComponentPtr<TransformComponent>;
            transform_desc.remove = &RemoveComponent<TransformComponent>;
            transform_desc.draw_custom = &DrawTransformComponent;
            descriptors.push_back(transform_desc);

            ComponentDesc camera_desc;
            camera_desc.name = "Camera";
            camera_desc.flags = ComponentFlags::Serializable | ComponentFlags::Addable;
            camera_desc.has = &HasComponent<CameraComponent>;
            camera_desc.get = &GetComponentPtr<CameraComponent>;
            camera_desc.remove = &RemoveComponent<CameraComponent>;
            camera_desc.properties = {
                PropertyDesc{
                    "Primary",
                    PropertyType::Bool,
                    offsetof(CameraComponent, Primary)
                },
                PropertyDesc{
                    "FovY",
                    PropertyType::Float,
                    offsetof(CameraComponent, FovY),
                    0.1f,
                    1.0f,
                    179.0f,
                    true,
                    "Vertical field of view in degrees."
                },
                PropertyDesc{
                    "Near",
                    PropertyType::Float,
                    offsetof(CameraComponent, Near),
                    0.01f,
                    0.001f,
                    100.0f,
                    true,
                    "Near clip plane distance."
                },
                PropertyDesc{
                    "Far",
                    PropertyType::Float,
                    offsetof(CameraComponent, Far),
                    1.0f,
                    1.0f,
                    10000.0f,
                    true,
                    "Far clip plane distance."
                }
            };
            descriptors.push_back(camera_desc);

            ComponentDesc directional_light_desc;
            directional_light_desc.name = "Directional Light";
            directional_light_desc.flags = ComponentFlags::Serializable | ComponentFlags::Addable;
            directional_light_desc.has = &HasComponent<DirectionalLightComponent>;
            directional_light_desc.get = &GetComponentPtr<DirectionalLightComponent>;
            directional_light_desc.remove = &RemoveComponent<DirectionalLightComponent>;
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
            point_light_desc.flags = ComponentFlags::Serializable | ComponentFlags::Addable;
            point_light_desc.has = &HasComponent<PointLightComponent>;
            point_light_desc.get = &GetComponentPtr<PointLightComponent>;
            point_light_desc.remove = &RemoveComponent<PointLightComponent>;
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

            ComponentDesc rigidbody_desc;
            rigidbody_desc.name = "Rigidbody";
            rigidbody_desc.flags = ComponentFlags::Serializable | ComponentFlags::Addable;
            rigidbody_desc.has = &HasComponent<RigidbodyComponent>;
            rigidbody_desc.get = &GetComponentPtr<RigidbodyComponent>;
            rigidbody_desc.remove = &RemoveComponent<RigidbodyComponent>;
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
            mesh_renderer_desc.flags = ComponentFlags::Serializable | ComponentFlags::Addable;
            mesh_renderer_desc.has = &HasComponent<MeshRendererComponent>;
            mesh_renderer_desc.get = &GetComponentPtr<MeshRendererComponent>;
            mesh_renderer_desc.remove = &RemoveComponent<MeshRendererComponent>;
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
