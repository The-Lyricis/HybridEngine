#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "runtime/core/reflection/reflection.h"
#include "runtime/modules/scene/components.h"
#include "runtime/modules/scene/entity.h"

namespace Hybrid
{
    enum class SceneComponentType : uint8_t
    {
        Tag = 0,
        Transform,
        Camera,
        MeshRenderer,
        DirectionalLight,
        PointLight,
        Collider,
        Rigidbody
    };

    using PropertyType = Reflect::ValueKind;
    using PropertyFlags = Reflect::FieldFlags;
    using PropertyDesc = Reflect::FieldInfo;

    enum class ComponentFlags : uint32_t
    {
        None = 0,
        Removable = 1 << 0,
        Serializable = 1 << 1,
        Addable = 1 << 2,
        ReadOnly = 1 << 3
    };

    constexpr ComponentFlags operator|(ComponentFlags lhs, ComponentFlags rhs)
    {
        return static_cast<ComponentFlags>(
            static_cast<uint32_t>(lhs) | static_cast<uint32_t>(rhs));
    }

    constexpr ComponentFlags operator&(ComponentFlags lhs, ComponentFlags rhs)
    {
        return static_cast<ComponentFlags>(
            static_cast<uint32_t>(lhs) & static_cast<uint32_t>(rhs));
    }

    constexpr bool HasAny(PropertyFlags value, PropertyFlags bits)
    {
        return Reflect::HasAny(value, bits);
    }

    constexpr bool HasAny(ComponentFlags value, ComponentFlags bits)
    {
        return static_cast<uint32_t>(value & bits) != 0;
    }
    using ComponentHasFn = bool (*)(Entity entity);
    using ComponentAddFn = bool (*)(Entity entity);
    using ComponentGetFn = void* (*)(Entity entity);
    using ComponentRemoveFn = void (*)(Entity entity);
    using ComponentEnabledFn = bool* (*)(void* componentPtr);
    using ComponentCopyFn = void (*)(Entity dst, Entity src);

    struct ComponentSchema
    {
        SceneComponentType type = SceneComponentType::Tag;
        const char* name = "";
        const char* serialization_key = "";
        ComponentFlags flags = ComponentFlags::None;
        const Reflect::TypeInfo* reflected_type = nullptr;

        ComponentHasFn has = nullptr;
        ComponentAddFn add = nullptr;
        ComponentGetFn get = nullptr;
        ComponentRemoveFn remove = nullptr;
        ComponentEnabledFn enabled = nullptr;
        ComponentCopyFn copy = nullptr;

        std::vector<PropertyDesc> properties;
        std::vector<std::vector<Reflect::AttributeInfo>> property_attributes;
    };

#define HYBRID_OPTIONAL_SCENE_COMPONENTS(X) \
    X(Camera, CameraComponent, camera, "Camera", "camera", ComponentFlags::Serializable | ComponentFlags::Addable | ComponentFlags::Removable) \
    X(MeshRenderer, MeshRendererComponent, mesh_renderer, "Mesh Renderer", "meshRenderer", ComponentFlags::Serializable | ComponentFlags::Addable | ComponentFlags::Removable) \
    X(DirectionalLight, DirectionalLightComponent, directional_light, "Directional Light", "dirLight", ComponentFlags::Serializable | ComponentFlags::Addable | ComponentFlags::Removable) \
    X(PointLight, PointLightComponent, point_light, "Point Light", "pointLight", ComponentFlags::Serializable | ComponentFlags::Addable | ComponentFlags::Removable) \
    X(Collider, ColliderComponent, collider, "BoxCollider", "collider", ComponentFlags::Serializable | ComponentFlags::Addable | ComponentFlags::Removable) \
    X(Rigidbody, RigidbodyComponent, rigidbody, "Rigidbody", "rigidbody", ComponentFlags::Serializable | ComponentFlags::Addable | ComponentFlags::Removable)

    const std::vector<ComponentSchema>& GetComponentSchemas();
    const ComponentSchema* FindComponentSchema(SceneComponentType type);
    void CopyOptionalSceneComponents(Entity dst, Entity src);
} // namespace Hybrid
