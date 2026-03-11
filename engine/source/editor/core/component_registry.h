#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "runtime/modules/scene/entity.h"

namespace Hybrid
{
    struct EditorContext;

    enum class PropertyType : uint8_t
    {
        Bool,
        Int,
        Float,
        String,
        Vec2,
        Vec3,
        Vec4,
        Enum,
        Asset,
        Custom
    };

    enum class PropertyFlags : uint32_t
    {
        None = 0,
        Visible = 1 << 0,
        Editable = 1 << 1,
        Serializable = 1 << 2,
        ReadOnly = 1 << 3,
        Color = 1 << 4,
        Angle = 1 << 5
    };

    enum class ComponentFlags : uint32_t
    {
        None = 0,
        Removable = 1 << 0,
        Serializable = 1 << 1,
        Addable = 1 << 2,
        ReadOnly = 1 << 3
    };

    constexpr PropertyFlags operator|(PropertyFlags lhs, PropertyFlags rhs)
    {
        return static_cast<PropertyFlags>(
            static_cast<uint32_t>(lhs) | static_cast<uint32_t>(rhs));
    }

    constexpr PropertyFlags operator&(PropertyFlags lhs, PropertyFlags rhs)
    {
        return static_cast<PropertyFlags>(
            static_cast<uint32_t>(lhs) & static_cast<uint32_t>(rhs));
    }

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
        return static_cast<uint32_t>(value & bits) != 0;
    }

    constexpr bool HasAny(ComponentFlags value, ComponentFlags bits)
    {
        return static_cast<uint32_t>(value & bits) != 0;
    }

    using PropertyDrawOverride = bool (*)(const char* label, void* valuePtr);
    using ComponentHasFn = bool (*)(Entity entity);
    using ComponentAddFn = bool (*)(Entity entity);
    using ComponentGetFn = void* (*)(Entity entity);
    using ComponentRemoveFn = void (*)(Entity entity);
    using ComponentEnabledFn = bool* (*)(void* componentPtr);
    using ComponentCustomDrawFn = bool (*)(EditorContext& ctx, Entity entity, void* componentPtr);

    struct PropertyDesc
    {
        const char* name = "";
        PropertyType type = PropertyType::Custom;
        size_t offset = 0;

        float speed = 0.1f;
        float min = 0.0f;
        float max = 0.0f;
        bool has_range = false;

        const char* tooltip = nullptr;
        PropertyFlags flags = PropertyFlags::Visible | PropertyFlags::Editable | PropertyFlags::Serializable;
        PropertyDrawOverride draw_override = nullptr;
    };

    struct ComponentDesc
    {
        const char* name = "";
        ComponentFlags flags = ComponentFlags::None;

        ComponentHasFn has = nullptr;
        ComponentAddFn add = nullptr;
        ComponentGetFn get = nullptr;
        ComponentRemoveFn remove = nullptr;
        ComponentEnabledFn enabled = nullptr;

        std::vector<PropertyDesc> properties;
        ComponentCustomDrawFn draw_custom = nullptr;
    };

    class ComponentRegistry
    {
    public:
        static const std::vector<ComponentDesc>& GetDescriptors();
    };
} // namespace Hybrid
