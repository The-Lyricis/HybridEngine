#include "reflection.h"

#include <cstring>
#include <string>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "runtime/modules/asset/asset_type.h"

namespace Hybrid::Reflect
{
    namespace
    {
        template<typename T>
        void Construct(void* dst)
        {
            new (dst) T();
        }

        template<typename T>
        void Destroy(void* dst)
        {
            static_cast<T*>(dst)->~T();
        }

        template<typename T>
        void CopyConstruct(void* dst, const void* src)
        {
            new (dst) T(*static_cast<const T*>(src));
        }

        template<typename T>
        void CopyAssign(void* dst, const void* src)
        {
            *static_cast<T*>(dst) = *static_cast<const T*>(src);
        }

        template<typename T>
        bool Equals(const void* lhs, const void* rhs)
        {
            return *static_cast<const T*>(lhs) == *static_cast<const T*>(rhs);
        }

        template<typename T>
        TypeInfo MakePrimitiveTypeInfo(const char* name, ValueKind value_kind, TypeKind type_kind = TypeKind::Primitive)
        {
            TypeInfo info{};
            info.name = name;
            info.kind = type_kind;
            info.value_kind = value_kind;
            info.size = sizeof(T);
            info.alignment = alignof(T);
            info.flags = TypeFlags::Reflectable | TypeFlags::Serializable;
            info.construct = &Construct<T>;
            info.destroy = &Destroy<T>;
            info.copy_construct = &CopyConstruct<T>;
            info.copy_assign = &CopyAssign<T>;
            info.equals = &Equals<T>;
            info.id = static_cast<TypeId>(std::hash<std::string_view>{}(name));
            return info;
        }
    } // namespace

    void* FieldInfo::getMutablePtr(void* instance) const
    {
        if (accessor != nullptr && accessor->get_mutable_ptr != nullptr)
            return accessor->get_mutable_ptr(instance);

        if (instance == nullptr)
            return nullptr;
        return static_cast<void*>(static_cast<char*>(instance) + offset);
    }

    const void* FieldInfo::getConstPtr(const void* instance) const
    {
        if (accessor != nullptr && accessor->get_const_ptr != nullptr)
            return accessor->get_const_ptr(instance);

        if (instance == nullptr)
            return nullptr;
        return static_cast<const void*>(static_cast<const char*>(instance) + offset);
    }

    ValueKind FieldInfo::resolvedValueKind() const
    {
        if (value_type != nullptr && value_type->value_kind != ValueKind::Custom)
            return value_type->value_kind;
        return ValueKind::Custom;
    }

    const AttributeInfo* FieldInfo::findAttribute(AttributeKind kind) const
    {
        for (size_t i = 0; i < attribute_count; ++i)
        {
            if (attributes[i].kind == kind)
                return &attributes[i];
        }
        return nullptr;
    }

    const char* FieldInfo::label() const
    {
        if (const AttributeInfo* attr = findAttribute(AttributeKind::DisplayName); attr != nullptr && attr->text != nullptr)
            return attr->text;
        return name;
    }

    const char* FieldInfo::tooltipText() const
    {
        if (const AttributeInfo* attr = findAttribute(AttributeKind::Tooltip); attr != nullptr && attr->text != nullptr)
            return attr->text;
        return nullptr;
    }

    float FieldInfo::stepValue() const
    {
        if (const AttributeInfo* attr = findAttribute(AttributeKind::Step); attr != nullptr)
            return attr->x;
        return 0.1f;
    }

    bool FieldInfo::hasRangeInfo() const
    {
        return findAttribute(AttributeKind::Range) != nullptr;
    }

    float FieldInfo::rangeMinValue() const
    {
        if (const AttributeInfo* attr = findAttribute(AttributeKind::Range); attr != nullptr)
            return attr->x;
        return 0.0f;
    }

    float FieldInfo::rangeMaxValue() const
    {
        if (const AttributeInfo* attr = findAttribute(AttributeKind::Range); attr != nullptr)
            return attr->y;
        return 0.0f;
    }

    bool TypeInfo::isEnum() const
    {
        return kind == TypeKind::Enum && enum_names != nullptr && enum_name_count > 0;
    }

    bool FieldInfo::isVisible() const
    {
        if (findAttribute(AttributeKind::Hidden) != nullptr)
            return false;
        return HasAny(flags, FieldFlags::Visible);
    }

    bool FieldInfo::isEditable() const
    {
        if (findAttribute(AttributeKind::ReadOnly) != nullptr)
            return false;
        return HasAny(flags, FieldFlags::Editable) && !HasAny(flags, FieldFlags::ReadOnly);
    }

    bool FieldInfo::isReadOnly() const
    {
        return findAttribute(AttributeKind::ReadOnly) != nullptr || HasAny(flags, FieldFlags::ReadOnly);
    }

    bool FieldInfo::isSerializable() const
    {
        if (const AttributeInfo* attr = findAttribute(AttributeKind::Serialize); attr != nullptr)
            return attr->u64 != 0;
        return HasAny(flags, FieldFlags::Serializable);
    }

    bool FieldInfo::isTransient() const
    {
        return findAttribute(AttributeKind::Transient) != nullptr || HasAny(flags, FieldFlags::Transient);
    }

    bool FieldInfo::isUndoable() const
    {
        if (const AttributeInfo* attr = findAttribute(AttributeKind::Undoable); attr != nullptr)
            return attr->u64 != 0;
        return HasAny(flags, FieldFlags::Undoable);
    }

    bool FieldInfo::isColorField() const
    {
        return findAttribute(AttributeKind::Color) != nullptr || HasAny(flags, FieldFlags::Color);
    }

    bool FieldInfo::isAngleField() const
    {
        return findAttribute(AttributeKind::Angle) != nullptr || HasAny(flags, FieldFlags::Angle);
    }

    uint64_t FieldInfo::assetTypeHint() const
    {
        if (const AttributeInfo* attr = findAttribute(AttributeKind::AssetType); attr != nullptr)
            return attr->u64;
        return 0;
    }

    const char* FieldInfo::assetLabelResolverKey() const
    {
        if (const AttributeInfo* attr = findAttribute(AttributeKind::AssetLabelResolver); attr != nullptr)
            return attr->text;
        return nullptr;
    }

    const TypeInfo* GetBoolTypeInfo()
    {
        static const TypeInfo s_info = MakePrimitiveTypeInfo<bool>("bool", ValueKind::Bool);
        return &s_info;
    }

    const TypeInfo* GetIntTypeInfo()
    {
        static const TypeInfo s_info = MakePrimitiveTypeInfo<int>("int", ValueKind::Int);
        return &s_info;
    }

    const TypeInfo* GetFloatTypeInfo()
    {
        static const TypeInfo s_info = MakePrimitiveTypeInfo<float>("float", ValueKind::Float);
        return &s_info;
    }

    const TypeInfo* GetStringTypeInfo()
    {
        static const TypeInfo s_info = MakePrimitiveTypeInfo<std::string>("std::string", ValueKind::String);
        return &s_info;
    }

    const TypeInfo* GetVec2TypeInfo()
    {
        static const TypeInfo s_info = MakePrimitiveTypeInfo<glm::vec2>("glm::vec2", ValueKind::Vec2);
        return &s_info;
    }

    const TypeInfo* GetVec3TypeInfo()
    {
        static const TypeInfo s_info = MakePrimitiveTypeInfo<glm::vec3>("glm::vec3", ValueKind::Vec3);
        return &s_info;
    }

    const TypeInfo* GetVec4TypeInfo()
    {
        static const TypeInfo s_info = MakePrimitiveTypeInfo<glm::vec4>("glm::vec4", ValueKind::Vec4);
        return &s_info;
    }

    const TypeInfo* GetAssetIdTypeInfo()
    {
        static const TypeInfo s_info = MakePrimitiveTypeInfo<Hybrid::AssetID>("AssetID", ValueKind::Asset, TypeKind::AssetRef);
        return &s_info;
    }

    const std::vector<const TypeInfo*>& GetBuiltinTypeInfos()
    {
        static const std::vector<const TypeInfo*> s_types = {
            GetBoolTypeInfo(),
            GetIntTypeInfo(),
            GetFloatTypeInfo(),
            GetStringTypeInfo(),
            GetVec2TypeInfo(),
            GetVec3TypeInfo(),
            GetVec4TypeInfo(),
            GetAssetIdTypeInfo()
        };
        return s_types;
    }

    const TypeInfo* FindBuiltinTypeByName(std::string_view name)
    {
        for (const TypeInfo* type : GetBuiltinTypeInfos())
        {
            if (type != nullptr && name == type->name)
                return type;
        }
        return nullptr;
    }
} // namespace Hybrid::Reflect
