#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace Hybrid::Reflect
{
    using TypeId = uint64_t;

    enum class ValueKind : uint8_t
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

    enum class TypeKind : uint8_t
    {
        Primitive,
        Enum,
        Struct,
        Component,
        AssetRef,
        Custom
    };

    enum class FieldFlags : uint32_t
    {
        None = 0,
        Visible = 1 << 0,
        Editable = 1 << 1,
        Serializable = 1 << 2,
        ReadOnly = 1 << 3,
        Color = 1 << 4,
        Angle = 1 << 5,
        Transient = 1 << 6,
        Undoable = 1 << 7
    };

    enum class TypeFlags : uint32_t
    {
        None = 0,
        Reflectable = 1 << 0,
        Serializable = 1 << 1,
        Component = 1 << 2,
        Asset = 1 << 3,
        EditorOnly = 1 << 4
    };

    enum class AttributeKind : uint8_t
    {
        DisplayName,
        Tooltip,
        Range,
        Step,
        ReadOnly,
        Hidden,
        Transient,
        Serialize,
        Undoable,
        AssetType,
        AssetLabelResolver,
        Angle,
        Color
    };

    constexpr FieldFlags operator|(FieldFlags lhs, FieldFlags rhs)
    {
        return static_cast<FieldFlags>(
            static_cast<uint32_t>(lhs) | static_cast<uint32_t>(rhs));
    }

    constexpr FieldFlags operator&(FieldFlags lhs, FieldFlags rhs)
    {
        return static_cast<FieldFlags>(
            static_cast<uint32_t>(lhs) & static_cast<uint32_t>(rhs));
    }

    constexpr TypeFlags operator|(TypeFlags lhs, TypeFlags rhs)
    {
        return static_cast<TypeFlags>(
            static_cast<uint32_t>(lhs) | static_cast<uint32_t>(rhs));
    }

    constexpr TypeFlags operator&(TypeFlags lhs, TypeFlags rhs)
    {
        return static_cast<TypeFlags>(
            static_cast<uint32_t>(lhs) & static_cast<uint32_t>(rhs));
    }

    constexpr bool HasAny(FieldFlags value, FieldFlags bits)
    {
        return static_cast<uint32_t>(value & bits) != 0;
    }

    constexpr bool HasAny(TypeFlags value, TypeFlags bits)
    {
        return static_cast<uint32_t>(value & bits) != 0;
    }

    struct AttributeInfo
    {
        AttributeKind kind = AttributeKind::DisplayName;
        const char* text = nullptr;
        float x = 0.0f;
        float y = 0.0f;
        uint64_t u64 = 0;
    };

    struct TypeInfo;

    struct ValueRef
    {
        const TypeInfo* type = nullptr;
        void* data = nullptr;
    };

    struct ConstValueRef
    {
        const TypeInfo* type = nullptr;
        const void* data = nullptr;
    };

    using PropertyDrawOverride = bool (*)(const char* label, void* value_ptr);

    struct FieldAccessor
    {
        void* (*get_mutable_ptr)(void* instance) = nullptr;
        const void* (*get_const_ptr)(const void* instance) = nullptr;
    };

    struct FieldInfo
    {
        const char* name = "";
        const TypeInfo* owner_type = nullptr;
        const TypeInfo* value_type = nullptr;
        const FieldAccessor* accessor = nullptr;
        size_t offset = 0;
        FieldFlags flags = FieldFlags::Visible | FieldFlags::Editable | FieldFlags::Serializable;
        PropertyDrawOverride draw_override = nullptr;

        const AttributeInfo* attributes = nullptr;
        size_t attribute_count = 0;

        void* getMutablePtr(void* instance) const;
        const void* getConstPtr(const void* instance) const;
        ValueKind resolvedValueKind() const;
        const AttributeInfo* findAttribute(AttributeKind kind) const;
        const char* label() const;
        const char* tooltipText() const;
        float stepValue() const;
        bool hasRangeInfo() const;
        float rangeMinValue() const;
        float rangeMaxValue() const;
        bool isVisible() const;
        bool isEditable() const;
        bool isReadOnly() const;
        bool isSerializable() const;
        bool isTransient() const;
        bool isUndoable() const;
        bool isColorField() const;
        bool isAngleField() const;
        uint64_t assetTypeHint() const;
        const char* assetLabelResolverKey() const;
    };

    struct TypeInfo
    {
        TypeId id = 0;
        const char* name = "";
        TypeKind kind = TypeKind::Custom;
        ValueKind value_kind = ValueKind::Custom;
        size_t size = 0;
        size_t alignment = 0;
        TypeFlags flags = TypeFlags::None;

        const TypeInfo* base_type = nullptr;
        const FieldInfo* fields = nullptr;
        size_t field_count = 0;
        const char* const* enum_names = nullptr;
        size_t enum_name_count = 0;

        void (*construct)(void*) = nullptr;
        void (*destroy)(void*) = nullptr;
        void (*copy_construct)(void*, const void*) = nullptr;
        void (*copy_assign)(void*, const void*) = nullptr;
        bool (*equals)(const void*, const void*) = nullptr;

        bool isEnum() const;
    };

    template<typename Owner, typename FieldT, FieldT Owner::* Member>
    struct MemberFieldAccessor
    {
        static void* GetMutablePtr(void* instance)
        {
            if (instance == nullptr)
                return nullptr;
            return &(static_cast<Owner*>(instance)->*Member);
        }

        static const void* GetConstPtr(const void* instance)
        {
            if (instance == nullptr)
                return nullptr;
            return &(static_cast<const Owner*>(instance)->*Member);
        }

        static const FieldAccessor* Get()
        {
            static const FieldAccessor s_accessor{
                &GetMutablePtr,
                &GetConstPtr
            };
            return &s_accessor;
        }
    };

    const TypeInfo* GetBoolTypeInfo();
    const TypeInfo* GetIntTypeInfo();
    const TypeInfo* GetFloatTypeInfo();
    const TypeInfo* GetStringTypeInfo();
    const TypeInfo* GetVec2TypeInfo();
    const TypeInfo* GetVec3TypeInfo();
    const TypeInfo* GetVec4TypeInfo();
    const TypeInfo* GetAssetIdTypeInfo();
    const std::vector<const TypeInfo*>& GetBuiltinTypeInfos();
    const TypeInfo* FindBuiltinTypeByName(std::string_view name);
} // namespace Hybrid::Reflect
