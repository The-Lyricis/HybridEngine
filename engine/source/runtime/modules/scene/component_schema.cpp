#include "component_schema.h"

#include <algorithm>
#include <string>
#include <type_traits>

namespace Hybrid
{
    namespace
    {
        using namespace Reflect;

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
        bool* GetEnabledPtr(void* component_ptr)
        {
            if (component_ptr == nullptr)
                return nullptr;
            return &static_cast<T*>(component_ptr)->Enabled;
        }

        template<typename T>
        void CopyComponent(Entity dst, Entity src)
        {
            if (!dst || !src)
                return;

            if (src.HasComponent<T>())
            {
                if (dst.HasComponent<T>())
                    dst.GetComponent<T>() = src.GetComponent<T>();
                else
                    dst.AddComponent<T>(src.GetComponent<T>());
            }
            else if (dst.HasComponent<T>())
            {
                dst.RemoveComponent<T>();
            }
        }

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
        Reflect::TypeInfo MakeStructTypeInfo(const char* name, Reflect::TypeKind kind, Reflect::TypeFlags flags)
        {
            Reflect::TypeInfo info{};
            info.id = static_cast<Reflect::TypeId>(std::hash<std::string_view>{}(name));
            info.name = name;
            info.kind = kind;
            info.value_kind = Reflect::ValueKind::Custom;
            info.size = sizeof(T);
            info.alignment = alignof(T);
            info.flags = flags;
            info.construct = &Construct<T>;
            info.destroy = &Destroy<T>;
            info.copy_construct = &CopyConstruct<T>;
            info.copy_assign = &CopyAssign<T>;
            if constexpr (std::is_enum_v<T> || std::is_arithmetic_v<T>)
                info.equals = &Equals<T>;
            else
                info.equals = nullptr;
            return info;
        }

        const Reflect::TypeInfo* GetCameraClearModeTypeInfo()
        {
            static const char* s_names[] = {
                "Solid Color",
                "Skybox"
            };
            static Reflect::TypeInfo s_info = MakeStructTypeInfo<CameraClearMode>(
                "CameraClearMode",
                Reflect::TypeKind::Enum,
                Reflect::TypeFlags::Reflectable | Reflect::TypeFlags::Serializable);
            s_info.value_kind = Reflect::ValueKind::Enum;
            s_info.enum_names = s_names;
            s_info.enum_name_count = std::size(s_names);
            return &s_info;
        }

        const Reflect::TypeInfo* GetColliderTypeTypeInfo()
        {
            static const char* s_names[] = {
                "None",
                "Box",
                "Sphere"
            };
            static Reflect::TypeInfo s_info = MakeStructTypeInfo<ColliderType>(
                "ColliderType",
                Reflect::TypeKind::Enum,
                Reflect::TypeFlags::Reflectable | Reflect::TypeFlags::Serializable);
            s_info.value_kind = Reflect::ValueKind::Enum;
            s_info.enum_names = s_names;
            s_info.enum_name_count = std::size(s_names);
            return &s_info;
        }

        template<typename T>
        const Reflect::TypeInfo* GetValueTypeInfo();

        template<> const Reflect::TypeInfo* GetValueTypeInfo<bool>() { return Reflect::GetBoolTypeInfo(); }
        template<> const Reflect::TypeInfo* GetValueTypeInfo<int>() { return Reflect::GetIntTypeInfo(); }
        template<> const Reflect::TypeInfo* GetValueTypeInfo<float>() { return Reflect::GetFloatTypeInfo(); }
        template<> const Reflect::TypeInfo* GetValueTypeInfo<std::string>() { return Reflect::GetStringTypeInfo(); }
        template<> const Reflect::TypeInfo* GetValueTypeInfo<glm::vec2>() { return Reflect::GetVec2TypeInfo(); }
        template<> const Reflect::TypeInfo* GetValueTypeInfo<glm::vec3>() { return Reflect::GetVec3TypeInfo(); }
        template<> const Reflect::TypeInfo* GetValueTypeInfo<glm::vec4>() { return Reflect::GetVec4TypeInfo(); }
        template<> const Reflect::TypeInfo* GetValueTypeInfo<AssetID>() { return Reflect::GetAssetIdTypeInfo(); }
        template<> const Reflect::TypeInfo* GetValueTypeInfo<CameraClearMode>() { return GetCameraClearModeTypeInfo(); }
        template<> const Reflect::TypeInfo* GetValueTypeInfo<ColliderType>() { return GetColliderTypeTypeInfo(); }

        struct PropertySpec
        {
            PropertyDesc field{};
            std::vector<Reflect::AttributeInfo> attributes;
        };

        void PopulateCommonAttributes(PropertySpec& spec,
                                      const char* display_name,
                                      float speed,
                                      float min,
                                      float max,
                                      bool has_range,
                                      const char* tooltip,
                                      PropertyFlags flags)
        {
            if (display_name != nullptr)
                spec.attributes.push_back(Reflect::AttributeInfo{Reflect::AttributeKind::DisplayName, display_name});
            if (tooltip != nullptr)
                spec.attributes.push_back(Reflect::AttributeInfo{Reflect::AttributeKind::Tooltip, tooltip});
            spec.attributes.push_back(Reflect::AttributeInfo{Reflect::AttributeKind::Step, nullptr, speed});
            if (has_range)
                spec.attributes.push_back(Reflect::AttributeInfo{Reflect::AttributeKind::Range, nullptr, min, max});
            spec.attributes.push_back(Reflect::AttributeInfo{
                Reflect::AttributeKind::Serialize,
                nullptr,
                0.0f,
                0.0f,
                Reflect::HasAny(flags, PropertyFlags::Serializable) ? 1ull : 0ull});
            if (Reflect::HasAny(flags, PropertyFlags::ReadOnly))
                spec.attributes.push_back(Reflect::AttributeInfo{Reflect::AttributeKind::ReadOnly});
            if (!Reflect::HasAny(flags, PropertyFlags::Visible))
                spec.attributes.push_back(Reflect::AttributeInfo{Reflect::AttributeKind::Hidden});
            if (Reflect::HasAny(flags, PropertyFlags::Transient))
                spec.attributes.push_back(Reflect::AttributeInfo{Reflect::AttributeKind::Transient});
            spec.attributes.push_back(Reflect::AttributeInfo{
                Reflect::AttributeKind::Undoable,
                nullptr,
                0.0f,
                0.0f,
                Reflect::HasAny(flags, PropertyFlags::Undoable) ? 1ull : 0ull});
            if (Reflect::HasAny(flags, PropertyFlags::Color))
                spec.attributes.push_back(Reflect::AttributeInfo{Reflect::AttributeKind::Color});
            if (Reflect::HasAny(flags, PropertyFlags::Angle))
                spec.attributes.push_back(Reflect::AttributeInfo{Reflect::AttributeKind::Angle});
        }

        void AddAssetAttributes(PropertySpec& spec,
                                AssetType asset_type,
                                const char* label_resolver_key = nullptr)
        {
            spec.attributes.push_back(Reflect::AttributeInfo{
                Reflect::AttributeKind::AssetType,
                nullptr,
                0.0f,
                0.0f,
                static_cast<uint64_t>(asset_type)});
            if (label_resolver_key != nullptr)
                spec.attributes.push_back(Reflect::AttributeInfo{
                    Reflect::AttributeKind::AssetLabelResolver,
                    label_resolver_key});
        }

        template<typename Owner, typename FieldT, FieldT Owner::* Member>
        PropertySpec MakeProperty(const char* name,
                                  const char* display_name,
                                  PropertyType type,
                                  float speed = 0.1f,
                                  float min = 0.0f,
                                  float max = 0.0f,
                                  bool has_range = false,
                                  const char* tooltip = nullptr,
                                  PropertyFlags flags = PropertyFlags::Visible | PropertyFlags::Editable | PropertyFlags::Serializable)
        {
            (void)type;
            PropertySpec spec{};
            PropertyDesc& property = spec.field;
            property.name = name;
            property.value_type = GetValueTypeInfo<FieldT>();
            property.accessor = Reflect::MemberFieldAccessor<Owner, FieldT, Member>::Get();
            property.offset = 0;
            property.flags = flags;
            PopulateCommonAttributes(spec, display_name, speed, min, max, has_range, tooltip, flags);
            return spec;
        }

        template<typename Owner, typename FieldT>
        PropertySpec MakePropertyWithOffset(const char* name,
                                            const char* display_name,
                                            PropertyType type,
                                            size_t offset,
                                            float speed = 0.1f,
                                            float min = 0.0f,
                                            float max = 0.0f,
                                            bool has_range = false,
                                            const char* tooltip = nullptr,
                                            PropertyFlags flags = PropertyFlags::Visible | PropertyFlags::Editable | PropertyFlags::Serializable)
        {
            (void)type;
            PropertySpec spec{};
            PropertyDesc& property = spec.field;
            property.name = name;
            property.value_type = GetValueTypeInfo<FieldT>();
            property.offset = offset;
            property.flags = flags;
            PopulateCommonAttributes(spec, display_name, speed, min, max, has_range, tooltip, flags);
            return spec;
        }

        Reflect::TypeInfo* GetTagComponentTypeInfo()
        {
            static Reflect::TypeInfo s_info = MakeStructTypeInfo<TagComponent>(
                "TagComponent",
                Reflect::TypeKind::Component,
                Reflect::TypeFlags::Reflectable | Reflect::TypeFlags::Serializable | Reflect::TypeFlags::Component);
            return &s_info;
        }

        Reflect::TypeInfo* GetTransformComponentTypeInfo()
        {
            static Reflect::TypeInfo s_info = MakeStructTypeInfo<TransformComponent>(
                "TransformComponent",
                Reflect::TypeKind::Component,
                Reflect::TypeFlags::Reflectable | Reflect::TypeFlags::Serializable | Reflect::TypeFlags::Component);
            return &s_info;
        }

        Reflect::TypeInfo* GetCameraComponentTypeInfo()
        {
            static Reflect::TypeInfo s_info = MakeStructTypeInfo<CameraComponent>(
                "CameraComponent",
                Reflect::TypeKind::Component,
                Reflect::TypeFlags::Reflectable | Reflect::TypeFlags::Serializable | Reflect::TypeFlags::Component);
            return &s_info;
        }

        Reflect::TypeInfo* GetMeshRendererComponentTypeInfo()
        {
            static Reflect::TypeInfo s_info = MakeStructTypeInfo<MeshRendererComponent>(
                "MeshRendererComponent",
                Reflect::TypeKind::Component,
                Reflect::TypeFlags::Reflectable | Reflect::TypeFlags::Serializable | Reflect::TypeFlags::Component);
            return &s_info;
        }

        Reflect::TypeInfo* GetDirectionalLightComponentTypeInfo()
        {
            static Reflect::TypeInfo s_info = MakeStructTypeInfo<DirectionalLightComponent>(
                "DirectionalLightComponent",
                Reflect::TypeKind::Component,
                Reflect::TypeFlags::Reflectable | Reflect::TypeFlags::Serializable | Reflect::TypeFlags::Component);
            return &s_info;
        }

        Reflect::TypeInfo* GetPointLightComponentTypeInfo()
        {
            static Reflect::TypeInfo s_info = MakeStructTypeInfo<PointLightComponent>(
                "PointLightComponent",
                Reflect::TypeKind::Component,
                Reflect::TypeFlags::Reflectable | Reflect::TypeFlags::Serializable | Reflect::TypeFlags::Component);
            return &s_info;
        }

        Reflect::TypeInfo* GetColliderComponentTypeInfo()
        {
            static Reflect::TypeInfo s_info = MakeStructTypeInfo<ColliderComponent>(
                "ColliderComponent",
                Reflect::TypeKind::Component,
                Reflect::TypeFlags::Reflectable | Reflect::TypeFlags::Serializable | Reflect::TypeFlags::Component);
            return &s_info;
        }

        Reflect::TypeInfo* GetRigidbodyComponentTypeInfo()
        {
            static Reflect::TypeInfo s_info = MakeStructTypeInfo<RigidbodyComponent>(
                "RigidbodyComponent",
                Reflect::TypeKind::Component,
                Reflect::TypeFlags::Reflectable | Reflect::TypeFlags::Serializable | Reflect::TypeFlags::Component);
            return &s_info;
        }

        void FinalizeSchema(ComponentSchema& schema, std::vector<PropertySpec>&& properties = {})
        {
            schema.properties.clear();
            schema.property_attributes.clear();
            schema.properties.reserve(properties.size());
            schema.property_attributes.reserve(properties.size());

            for (PropertySpec& spec : properties)
            {
                schema.property_attributes.push_back(std::move(spec.attributes));
                PropertyDesc property = std::move(spec.field);
                property.owner_type = schema.reflected_type;
                const auto& attrs = schema.property_attributes.back();
                property.attributes = attrs.empty() ? nullptr : attrs.data();
                property.attribute_count = attrs.size();
                schema.properties.push_back(std::move(property));
            }

            if (schema.reflected_type != nullptr)
            {
                auto* reflected_type = const_cast<Reflect::TypeInfo*>(schema.reflected_type);
                reflected_type->fields = schema.properties.empty() ? nullptr : schema.properties.data();
                reflected_type->field_count = schema.properties.size();
            }
        }

        ComponentSchema MakeTagSchema()
        {
            ComponentSchema schema{};
            schema.type = SceneComponentType::Tag;
            schema.name = "Tag";
            schema.serialization_key = "tag";
            schema.flags = ComponentFlags::Serializable;
            schema.reflected_type = GetTagComponentTypeInfo();
            schema.has = &HasComponent<TagComponent>;
            schema.add = &AddComponent<TagComponent>;
            schema.get = &GetComponentPtr<TagComponent>;
            schema.remove = &RemoveComponent<TagComponent>;
            schema.copy = &CopyComponent<TagComponent>;
            std::vector<PropertySpec> properties = {
                MakePropertyWithOffset<TagComponent, std::string>(
                    "Name",
                    "Name",
                    PropertyType::String,
                    offsetof(TagComponent, Tag))
            };
            FinalizeSchema(schema, std::move(properties));
            return schema;
        }

        ComponentSchema MakeTransformSchema()
        {
            ComponentSchema schema{};
            schema.type = SceneComponentType::Transform;
            schema.name = "Transform";
            schema.serialization_key = "transform";
            schema.flags = ComponentFlags::Serializable;
            schema.reflected_type = GetTransformComponentTypeInfo();
            schema.has = &HasComponent<TransformComponent>;
            schema.add = &AddComponent<TransformComponent>;
            schema.get = &GetComponentPtr<TransformComponent>;
            schema.remove = &RemoveComponent<TransformComponent>;
            schema.copy = &CopyComponent<TransformComponent>;
            FinalizeSchema(schema, {});
            return schema;
        }

        ComponentSchema MakeCameraSchema()
        {
            ComponentSchema schema{};
            schema.type = SceneComponentType::Camera;
            schema.name = "Camera";
            schema.serialization_key = "camera";
            schema.flags = ComponentFlags::Serializable | ComponentFlags::Addable | ComponentFlags::Removable;
            schema.reflected_type = GetCameraComponentTypeInfo();
            schema.has = &HasComponent<CameraComponent>;
            schema.add = &AddComponent<CameraComponent>;
            schema.get = &GetComponentPtr<CameraComponent>;
            schema.remove = &RemoveComponent<CameraComponent>;
            schema.enabled = &GetEnabledPtr<CameraComponent>;
            schema.copy = &CopyComponent<CameraComponent>;
            std::vector<PropertySpec> properties = {
                MakeProperty<CameraComponent, bool, &CameraComponent::Primary>("primary", "Primary", PropertyType::Bool),
                MakeProperty<CameraComponent, float, &CameraComponent::FovY>("fovY", "FovY", PropertyType::Float, 0.1f, 1.0f, 179.0f, true),
                MakeProperty<CameraComponent, float, &CameraComponent::Near>("near", "Near", PropertyType::Float, 0.01f, 0.001f, 100.0f, true),
                MakeProperty<CameraComponent, float, &CameraComponent::Far>("far", "Far", PropertyType::Float, 1.0f, 1.0f, 10000.0f, true),
                MakeProperty<CameraComponent, CameraClearMode, &CameraComponent::ClearMode>("clearMode", "Clear Mode", PropertyType::Enum),
                MakeProperty<CameraComponent, glm::vec4, &CameraComponent::ClearColor>("clearColor", "Clear Color", PropertyType::Vec4, 0.1f, 0.0f, 0.0f, false, nullptr, PropertyFlags::Visible | PropertyFlags::Editable | PropertyFlags::Serializable | PropertyFlags::Color)
            };
            FinalizeSchema(schema, std::move(properties));
            return schema;
        }

        ComponentSchema MakeMeshRendererSchema()
        {
            ComponentSchema schema{};
            schema.type = SceneComponentType::MeshRenderer;
            schema.name = "Mesh Renderer";
            schema.serialization_key = "meshRenderer";
            schema.flags = ComponentFlags::Serializable | ComponentFlags::Addable | ComponentFlags::Removable;
            schema.reflected_type = GetMeshRendererComponentTypeInfo();
            schema.has = &HasComponent<MeshRendererComponent>;
            schema.add = &AddComponent<MeshRendererComponent>;
            schema.get = &GetComponentPtr<MeshRendererComponent>;
            schema.remove = &RemoveComponent<MeshRendererComponent>;
            schema.enabled = &GetEnabledPtr<MeshRendererComponent>;
            schema.copy = &CopyComponent<MeshRendererComponent>;
            std::vector<PropertySpec> properties = {
                MakeProperty<MeshRendererComponent, AssetID, &MeshRendererComponent::Mesh>("mesh", "Mesh", PropertyType::Asset),
                MakeProperty<MeshRendererComponent, AssetID, &MeshRendererComponent::Material>("material", "Material", PropertyType::Asset),
                MakeProperty<MeshRendererComponent, glm::vec4, &MeshRendererComponent::Tint>("tint", "Tint", PropertyType::Vec4, 0.1f, 0.0f, 0.0f, false, nullptr, PropertyFlags::Visible | PropertyFlags::Editable | PropertyFlags::Serializable | PropertyFlags::Color)
            };
            AddAssetAttributes(properties[0], AssetType::Mesh);
            AddAssetAttributes(properties[1], AssetType::Material, "meshRendererMaterial");
            FinalizeSchema(schema, std::move(properties));
            return schema;
        }

        ComponentSchema MakeDirectionalLightSchema()
        {
            ComponentSchema schema{};
            schema.type = SceneComponentType::DirectionalLight;
            schema.name = "Directional Light";
            schema.serialization_key = "dirLight";
            schema.flags = ComponentFlags::Serializable | ComponentFlags::Addable | ComponentFlags::Removable;
            schema.reflected_type = GetDirectionalLightComponentTypeInfo();
            schema.has = &HasComponent<DirectionalLightComponent>;
            schema.add = &AddComponent<DirectionalLightComponent>;
            schema.get = &GetComponentPtr<DirectionalLightComponent>;
            schema.remove = &RemoveComponent<DirectionalLightComponent>;
            schema.enabled = &GetEnabledPtr<DirectionalLightComponent>;
            schema.copy = &CopyComponent<DirectionalLightComponent>;
            std::vector<PropertySpec> properties = {
                MakeProperty<DirectionalLightComponent, glm::vec3, &DirectionalLightComponent::Color>("color", "Color", PropertyType::Vec3, 0.1f, 0.0f, 0.0f, false, "Light color.", PropertyFlags::Visible | PropertyFlags::Editable | PropertyFlags::Serializable | PropertyFlags::Color),
                MakeProperty<DirectionalLightComponent, float, &DirectionalLightComponent::Intensity>("intensity", "Intensity", PropertyType::Float, 0.05f, 0.0f, 100.0f, true, "Directional light intensity.")
            };
            FinalizeSchema(schema, std::move(properties));
            return schema;
        }

        ComponentSchema MakePointLightSchema()
        {
            ComponentSchema schema{};
            schema.type = SceneComponentType::PointLight;
            schema.name = "Point Light";
            schema.serialization_key = "pointLight";
            schema.flags = ComponentFlags::Serializable | ComponentFlags::Addable | ComponentFlags::Removable;
            schema.reflected_type = GetPointLightComponentTypeInfo();
            schema.has = &HasComponent<PointLightComponent>;
            schema.add = &AddComponent<PointLightComponent>;
            schema.get = &GetComponentPtr<PointLightComponent>;
            schema.remove = &RemoveComponent<PointLightComponent>;
            schema.enabled = &GetEnabledPtr<PointLightComponent>;
            schema.copy = &CopyComponent<PointLightComponent>;
            std::vector<PropertySpec> properties = {
                MakeProperty<PointLightComponent, glm::vec3, &PointLightComponent::Color>("color", "Color", PropertyType::Vec3, 0.1f, 0.0f, 0.0f, false, "Light color.", PropertyFlags::Visible | PropertyFlags::Editable | PropertyFlags::Serializable | PropertyFlags::Color),
                MakeProperty<PointLightComponent, float, &PointLightComponent::Intensity>("intensity", "Intensity", PropertyType::Float, 0.05f, 0.0f, 100.0f, true, "Point light intensity."),
                MakeProperty<PointLightComponent, float, &PointLightComponent::Range>("range", "Range", PropertyType::Float, 0.1f, 0.0f, 1000.0f, true, "Point light attenuation range.")
            };
            FinalizeSchema(schema, std::move(properties));
            return schema;
        }

        ComponentSchema MakeColliderSchema()
        {
            ComponentSchema schema{};
            schema.type = SceneComponentType::Collider;
            schema.name = "BoxCollider";
            schema.serialization_key = "collider";
            schema.flags = ComponentFlags::Serializable | ComponentFlags::Addable | ComponentFlags::Removable;
            schema.reflected_type = GetColliderComponentTypeInfo();
            schema.has = &HasComponent<ColliderComponent>;
            schema.add = &AddComponent<ColliderComponent>;
            schema.get = &GetComponentPtr<ColliderComponent>;
            schema.remove = &RemoveComponent<ColliderComponent>;
            schema.enabled = &GetEnabledPtr<ColliderComponent>;
            schema.copy = &CopyComponent<ColliderComponent>;
            std::vector<PropertySpec> properties = {
                MakeProperty<ColliderComponent, bool, &ColliderComponent::IsTrigger>("isTrigger", "Is Trigger", PropertyType::Bool),
                MakeProperty<ColliderComponent, ColliderType, &ColliderComponent::Type>("type", "Type", PropertyType::Enum),
                MakeProperty<ColliderComponent, glm::vec3, &ColliderComponent::Center>("center", "Center", PropertyType::Vec3, 0.05f)
            };
            FinalizeSchema(schema, std::move(properties));
            return schema;
        }

        ComponentSchema MakeRigidbodySchema()
        {
            ComponentSchema schema{};
            schema.type = SceneComponentType::Rigidbody;
            schema.name = "Rigidbody";
            schema.serialization_key = "rigidbody";
            schema.flags = ComponentFlags::Serializable | ComponentFlags::Addable | ComponentFlags::Removable;
            schema.reflected_type = GetRigidbodyComponentTypeInfo();
            schema.has = &HasComponent<RigidbodyComponent>;
            schema.add = &AddComponent<RigidbodyComponent>;
            schema.get = &GetComponentPtr<RigidbodyComponent>;
            schema.remove = &RemoveComponent<RigidbodyComponent>;
            schema.enabled = &GetEnabledPtr<RigidbodyComponent>;
            schema.copy = &CopyComponent<RigidbodyComponent>;
            std::vector<PropertySpec> properties = {
                MakeProperty<RigidbodyComponent, glm::vec3, &RigidbodyComponent::Velocity>("velocity", "Velocity", PropertyType::Vec3, 0.05f, 0.0f, 0.0f, false, "Linear velocity in world space."),
                MakeProperty<RigidbodyComponent, glm::vec3, &RigidbodyComponent::ConstantForce>("constantForce", "Constant Force", PropertyType::Vec3, 0.05f, 0.0f, 0.0f, false, "Continuous force applied every physics step in world space."),
                MakeProperty<RigidbodyComponent, float, &RigidbodyComponent::Mass>("mass", "Mass", PropertyType::Float, 0.05f, 0.001f, 1000.0f, true, "Rigid body mass."),
                MakeProperty<RigidbodyComponent, bool, &RigidbodyComponent::UseGravity>("useGravity", "Use Gravity", PropertyType::Bool, 0.1f, 0.0f, 0.0f, false, "Whether gravity affects this body."),
                MakeProperty<RigidbodyComponent, bool, &RigidbodyComponent::IsKinematic>("isKinematic", "Is Kinematic", PropertyType::Bool, 0.1f, 0.0f, 0.0f, false, "Whether the rigid body is driven externally.")
            };
            FinalizeSchema(schema, std::move(properties));
            return schema;
        }

        std::vector<ComponentSchema> BuildSchemas()
        {
            std::vector<ComponentSchema> schemas;
            schemas.reserve(8);
            schemas.push_back(MakeTagSchema());
            schemas.push_back(MakeTransformSchema());
            schemas.push_back(MakeCameraSchema());
            schemas.push_back(MakeMeshRendererSchema());
            schemas.push_back(MakeDirectionalLightSchema());
            schemas.push_back(MakePointLightSchema());
            schemas.push_back(MakeColliderSchema());
            schemas.push_back(MakeRigidbodySchema());
            return schemas;
        }
    } // namespace

    const std::vector<ComponentSchema>& GetComponentSchemas()
    {
        static const std::vector<ComponentSchema> s_schemas = BuildSchemas();
        return s_schemas;
    }

    const ComponentSchema* FindComponentSchema(SceneComponentType type)
    {
        const std::vector<ComponentSchema>& schemas = GetComponentSchemas();
        const auto it = std::find_if(
            schemas.begin(),
            schemas.end(),
            [type](const ComponentSchema& schema) { return schema.type == type; });
        return it != schemas.end() ? &(*it) : nullptr;
    }

    void CopyOptionalSceneComponents(Entity dst, Entity src)
    {
#define HYBRID_COPY_OPTIONAL_COMPONENT(id, type, member, name, key, flags) \
        CopyComponent<type>(dst, src);
        HYBRID_OPTIONAL_SCENE_COMPONENTS(HYBRID_COPY_OPTIONAL_COMPONENT)
#undef HYBRID_COPY_OPTIONAL_COMPONENT
    }
} // namespace Hybrid
