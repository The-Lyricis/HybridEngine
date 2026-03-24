// scene_serializer.cpp
#include "scene_serializer.h"

#include <algorithm>
#include <fstream>
#include <string>
#include <type_traits>
#include <vector>
#include <nlohmann/json.hpp>

#include "runtime/modules/asset/asset_registry.h"
#include "component_schema.h"
#include "scene.h"
#include "components.h"
#include "uuid.h"

#include <unordered_map>

namespace Hybrid
{
    namespace
    {
        using json = nlohmann::json;

        // -----------------------------
        // glm helpers
        // -----------------------------
        static json toJson(const glm::vec3& v) { return json::array({ v.x, v.y, v.z }); }
        static json toJson(const glm::vec4& v) { return json::array({ v.x, v.y, v.z, v.w }); }
        static json toJson(const glm::quat& q) { return json::array({ q.w, q.x, q.y, q.z }); } // [w,x,y,z]

        static glm::vec3 vec3From(const json& j)
        {
            return glm::vec3(j.at(0).get<float>(), j.at(1).get<float>(), j.at(2).get<float>());
        }

        static glm::vec4 vec4From(const json& j)
        {
            return glm::vec4(j.at(0).get<float>(), j.at(1).get<float>(), j.at(2).get<float>(), j.at(3).get<float>());
        }

        static glm::quat quatFrom(const json& j)
        {
            return glm::quat(j.at(0).get<float>(), j.at(1).get<float>(), j.at(2).get<float>(), j.at(3).get<float>());
        }

        // -----------------------------
        // safe getters
        // -----------------------------
        static uint64_t getUUID(entt::registry& reg, entt::entity e)
        {
            if (!reg.valid(e) || !reg.all_of<IDComponent>(e)) return 0;
            return static_cast<uint64_t>(reg.get<IDComponent>(e).ID);
        }

        static std::string assetPathFor(const AssetRegistry* registry, AssetID id)
        {
            if (!registry || id.value == 0)
                return {};

            const auto* meta = registry->find(id);
            if (!meta || !meta->is_valid)
                return {};

            if (!meta->source_path.empty())
                return meta->source_path;
            return meta->cooked_path;
        }

        static AssetID resolveAsset(const json& j,
                                    const char* id_key,
                                    const char* path_key,
                                    const AssetRegistry* registry)
        {
            if (registry && j.contains(path_key) && j[path_key].is_string())
            {
                const std::string logical = j[path_key].get<std::string>();
                if (!logical.empty())
                {
                    if (const auto* meta = registry->findByPath(logical))
                        return meta->id;
                }
            }

            return AssetID::FromRaw(j.value(id_key, 0ull));
        }

        static const char* componentKey(SceneComponentType type)
        {
            if (const ComponentSchema* schema = FindComponentSchema(type))
                return schema->serialization_key;
            return "";
        }

        static json serializePropertyValue(const void* value_ptr, const PropertyDesc& property)
        {
            switch (property.resolvedValueKind())
            {
            case PropertyType::Bool:
                return *static_cast<const bool*>(value_ptr);
            case PropertyType::Int:
                return *static_cast<const int*>(value_ptr);
            case PropertyType::Float:
                return *static_cast<const float*>(value_ptr);
            case PropertyType::String:
                return *static_cast<const std::string*>(value_ptr);
            case PropertyType::Vec2:
            {
                const auto& v = *static_cast<const glm::vec2*>(value_ptr);
                return json::array({v.x, v.y});
            }
            case PropertyType::Vec3:
                return toJson(*static_cast<const glm::vec3*>(value_ptr));
            case PropertyType::Vec4:
                return toJson(*static_cast<const glm::vec4*>(value_ptr));
            case PropertyType::Enum:
                switch (property.value_type != nullptr ? property.value_type->size : 0)
                {
                case 1: return static_cast<int>(*static_cast<const uint8_t*>(value_ptr));
                case 2: return static_cast<int>(*static_cast<const uint16_t*>(value_ptr));
                case 4: return static_cast<int>(*static_cast<const uint32_t*>(value_ptr));
                case 8: return static_cast<int64_t>(*static_cast<const uint64_t*>(value_ptr));
                default: return json();
                }
            case PropertyType::Asset:
                return static_cast<const AssetID*>(value_ptr)->value;
            default:
                return json();
            }
        }

        static bool deserializePropertyValue(const json& value, void* value_ptr, const PropertyDesc& property)
        {
            switch (property.resolvedValueKind())
            {
            case PropertyType::Bool:
                if (!value.is_boolean()) return false;
                *static_cast<bool*>(value_ptr) = value.get<bool>();
                return true;
            case PropertyType::Int:
                if (!value.is_number_integer()) return false;
                *static_cast<int*>(value_ptr) = value.get<int>();
                return true;
            case PropertyType::Float:
                if (!value.is_number()) return false;
                *static_cast<float*>(value_ptr) = value.get<float>();
                return true;
            case PropertyType::String:
                if (!value.is_string()) return false;
                *static_cast<std::string*>(value_ptr) = value.get<std::string>();
                return true;
            case PropertyType::Vec2:
                if (!value.is_array() || value.size() != 2) return false;
                *static_cast<glm::vec2*>(value_ptr) =
                    glm::vec2(value.at(0).get<float>(), value.at(1).get<float>());
                return true;
            case PropertyType::Vec3:
                if (!value.is_array() || value.size() != 3) return false;
                *static_cast<glm::vec3*>(value_ptr) = vec3From(value);
                return true;
            case PropertyType::Vec4:
                if (!value.is_array() || value.size() != 4) return false;
                *static_cast<glm::vec4*>(value_ptr) = vec4From(value);
                return true;
            case PropertyType::Enum:
                if (!value.is_number_integer()) return false;
                switch (property.value_type != nullptr ? property.value_type->size : 0)
                {
                case 1: *static_cast<uint8_t*>(value_ptr) = static_cast<uint8_t>(value.get<int>()); return true;
                case 2: *static_cast<uint16_t*>(value_ptr) = static_cast<uint16_t>(value.get<int>()); return true;
                case 4: *static_cast<uint32_t*>(value_ptr) = static_cast<uint32_t>(value.get<int>()); return true;
                case 8: *static_cast<uint64_t*>(value_ptr) = static_cast<uint64_t>(value.get<int64_t>()); return true;
                default: return false;
                }
            case PropertyType::Asset:
                if (!value.is_number_unsigned() && !value.is_number_integer()) return false;
                *static_cast<AssetID*>(value_ptr) = AssetID::FromRaw(value.get<uint64_t>());
                return true;
            default:
                return false;
            }
        }

        static void writeSchemaProperties(json& out,
                                          const void* component_ptr,
                                          const ComponentSchema& schema)
        {
            if (component_ptr == nullptr)
                return;

            if (schema.enabled != nullptr)
            {
                const bool* enabled = schema.enabled(const_cast<void*>(component_ptr));
                if (enabled != nullptr)
                    out["enabled"] = *enabled;
            }

            for (const PropertyDesc& property : schema.properties)
            {
                if (!property.isSerializable() || property.isTransient())
                    continue;

                const void* value_ptr = property.getConstPtr(component_ptr);
                if (value_ptr == nullptr)
                    continue;
                out[property.name] = serializePropertyValue(value_ptr, property);
            }
        }

        static void readSchemaProperties(const json& in,
                                         void* component_ptr,
                                         const ComponentSchema& schema)
        {
            if (component_ptr == nullptr)
                return;

            if (schema.enabled != nullptr)
            {
                if (bool* enabled = schema.enabled(component_ptr); enabled != nullptr)
                    *enabled = in.value("enabled", true);
            }

            for (const PropertyDesc& property : schema.properties)
            {
                if (!property.isSerializable() || property.isTransient())
                    continue;
                if (!in.contains(property.name))
                    continue;

                void* value_ptr = property.getMutablePtr(component_ptr);
                if (value_ptr == nullptr)
                    continue;
                (void)deserializePropertyValue(in[property.name], value_ptr, property);
            }
        }

        static void writeComponents(entt::registry& reg, entt::entity e, json& je, const AssetRegistry* registry)
        {
            // Camera
            if (reg.all_of<CameraComponent>(e))
            {
                const auto& c = reg.get<CameraComponent>(e);
                json jc = json::object();
                if (const ComponentSchema* schema = FindComponentSchema(SceneComponentType::Camera))
                    writeSchemaProperties(jc, &c, *schema);
                je[componentKey(SceneComponentType::Camera)] = std::move(jc);
            }

            // MeshRenderer
            if (reg.all_of<MeshRendererComponent>(e))
            {
                const auto& mr = reg.get<MeshRendererComponent>(e);
                json jm = json::object();
                if (const ComponentSchema* schema = FindComponentSchema(SceneComponentType::MeshRenderer))
                    writeSchemaProperties(jm, &mr, *schema);

                const std::string mesh_path = assetPathFor(registry, mr.Mesh);
                if (!mesh_path.empty())
                    jm["meshPath"] = mesh_path;

                const std::string material_path = assetPathFor(registry, mr.Material);
                if (!material_path.empty())
                    jm["materialPath"] = material_path;
                je[componentKey(SceneComponentType::MeshRenderer)] = std::move(jm);
            }

            // DirectionalLight
            if (reg.all_of<DirectionalLightComponent>(e))
            {
                const auto& dl = reg.get<DirectionalLightComponent>(e);
                json jl = json::object();
                if (const ComponentSchema* schema = FindComponentSchema(SceneComponentType::DirectionalLight))
                    writeSchemaProperties(jl, &dl, *schema);
                je[componentKey(SceneComponentType::DirectionalLight)] = std::move(jl);
            }

            // PointLight
            if (reg.all_of<PointLightComponent>(e))
            {
                const auto& pl = reg.get<PointLightComponent>(e);
                json jl = json::object();
                if (const ComponentSchema* schema = FindComponentSchema(SceneComponentType::PointLight))
                    writeSchemaProperties(jl, &pl, *schema);
                je[componentKey(SceneComponentType::PointLight)] = std::move(jl);
            }

            if (reg.all_of<ColliderComponent>(e))
            {
                const auto& collider = reg.get<ColliderComponent>(e);
                json jc = json::object();
                if (const ComponentSchema* schema = FindComponentSchema(SceneComponentType::Collider))
                    writeSchemaProperties(jc, &collider, *schema);

                if (collider.Type == ColliderType::Box)
                    jc["halfExtents"] = toJson(collider.Box.HalfExtents);
                else if (collider.Type == ColliderType::Sphere)
                    jc["radius"] = collider.Sphere.Radius;

                je[componentKey(SceneComponentType::Collider)] = std::move(jc);
            }

            if (reg.all_of<RigidbodyComponent>(e))
            {
                const auto& rb = reg.get<RigidbodyComponent>(e);
                json jr = json::object();
                if (const ComponentSchema* schema = FindComponentSchema(SceneComponentType::Rigidbody))
                    writeSchemaProperties(jr, &rb, *schema);
                je[componentKey(SceneComponentType::Rigidbody)] = std::move(jr);
            }
        }

        static void readComponents(entt::registry& reg, entt::entity e, const json& je, const AssetRegistry* registry)
        {
            // Camera
            if (const char* key = componentKey(SceneComponentType::Camera); je.contains(key) && je[key].is_object())
            {
                const auto& jc = je[key];
                auto& c = reg.all_of<CameraComponent>(e) ? reg.get<CameraComponent>(e) : reg.emplace<CameraComponent>(e);
                if (const ComponentSchema* schema = FindComponentSchema(SceneComponentType::Camera))
                    readSchemaProperties(jc, &c, *schema);
            }

            // MeshRenderer
            if (const char* key = componentKey(SceneComponentType::MeshRenderer); je.contains(key) && je[key].is_object())
            {
                const auto& jm = je[key];
                auto& mr = reg.all_of<MeshRendererComponent>(e) ? reg.get<MeshRendererComponent>(e)
                    : reg.emplace<MeshRendererComponent>(e);
                if (const ComponentSchema* schema = FindComponentSchema(SceneComponentType::MeshRenderer))
                    readSchemaProperties(jm, &mr, *schema);
                mr.Mesh = resolveAsset(jm, "Mesh", "meshPath", registry);
                mr.Material = resolveAsset(jm, "Material", "materialPath", registry);
            }

            // DirectionalLight
            if (const char* key = componentKey(SceneComponentType::DirectionalLight); je.contains(key) && je[key].is_object())
            {
                const auto& jl = je[key];
                auto& dl = reg.all_of<DirectionalLightComponent>(e) ? reg.get<DirectionalLightComponent>(e)
                    : reg.emplace<DirectionalLightComponent>(e);
                if (const ComponentSchema* schema = FindComponentSchema(SceneComponentType::DirectionalLight))
                    readSchemaProperties(jl, &dl, *schema);
            }

            // PointLight
            if (const char* key = componentKey(SceneComponentType::PointLight); je.contains(key) && je[key].is_object())
            {
                const auto& jl = je[key];
                auto& pl = reg.all_of<PointLightComponent>(e) ? reg.get<PointLightComponent>(e)
                    : reg.emplace<PointLightComponent>(e);
                if (const ComponentSchema* schema = FindComponentSchema(SceneComponentType::PointLight))
                    readSchemaProperties(jl, &pl, *schema);
            }

            if (const char* key = componentKey(SceneComponentType::Collider); je.contains(key) && je[key].is_object())
            {
                const auto& jc = je[key];
                auto& collider = reg.all_of<ColliderComponent>(e) ? reg.get<ColliderComponent>(e)
                    : reg.emplace<ColliderComponent>(e);
                if (const ComponentSchema* schema = FindComponentSchema(SceneComponentType::Collider))
                    readSchemaProperties(jc, &collider, *schema);

                if (jc.contains("halfExtents") && jc["halfExtents"].is_array() && jc["halfExtents"].size() == 3)
                    collider.Box.HalfExtents = vec3From(jc["halfExtents"]);

                collider.Box.HalfExtents.x = std::max(0.0f, collider.Box.HalfExtents.x);
                collider.Box.HalfExtents.y = std::max(0.0f, collider.Box.HalfExtents.y);
                collider.Box.HalfExtents.z = std::max(0.0f, collider.Box.HalfExtents.z);

                collider.Sphere.Radius = std::max(0.0f, jc.value("radius", collider.Sphere.Radius));
            }

            if (const char* key = componentKey(SceneComponentType::Rigidbody); je.contains(key) && je[key].is_object())
            {
                const auto& jr = je[key];
                auto& rb = reg.all_of<RigidbodyComponent>(e) ? reg.get<RigidbodyComponent>(e)
                    : reg.emplace<RigidbodyComponent>(e);
                if (const ComponentSchema* schema = FindComponentSchema(SceneComponentType::Rigidbody))
                    readSchemaProperties(jr, &rb, *schema);
                rb.Force = glm::vec3(0.0f);
            }
        }

        // DFS write：保持层级顺序稳定（root -> children, sibling order）
        static void writeEntityDFS(const Scene& scene, entt::entity e, json& outEntities, const AssetRegistry* registry)
        {
            auto& reg = const_cast<entt::registry&>(scene.getRegistry());

            const auto& id = reg.get<IDComponent>(e);
            const auto& tag = reg.get<TagComponent>(e);
            const auto& tc = reg.get<TransformComponent>(e);

            uint64_t parent_uuid = 0;
            if (tc.Parent != entt::null)
                parent_uuid = getUUID(reg, tc.Parent);

            json je;
            je["uuid"] = static_cast<uint64_t>(id.ID);
            je["name"] = tag.Tag;
            je["parent"] = parent_uuid;

            je["transform"] = {
                {"t", toJson(tc.Position)},
                {"r", toJson(tc.Rotation)},
                {"s", toJson(tc.Scale)}
            };

            // 新增：写其它组件
            writeComponents(reg, e, je, registry);

            outEntities.push_back(std::move(je));

            // children
            entt::entity child = tc.FirstChild;
            while (child != entt::null)
            {
                entt::entity next = entt::null;
                if (reg.valid(child) && reg.all_of<TransformComponent>(child))
                    next = reg.get<TransformComponent>(child).NextSibling;

                writeEntityDFS(scene, child, outEntities, registry);
                child = next;
            }
        }

    } // namespace

    bool SceneSerializer::SerializeToFile(const Scene& scene, const std::filesystem::path& path, const AssetRegistry* registry)
    {
        // 让层级/矩阵状态先同步一次（不影响可持久化数据，只是避免脏状态）
        const_cast<Scene&>(scene).onUpdate(0.0f);

        json root;
        root["meta"] = { {"version", 2} };   // 升级版本：v2 支持更多组件
        root["environment"] = {
            {"skyboxCubemap", scene.environment().skybox_cubemap.value},
            {"skyboxCubemapPath", assetPathFor(registry, scene.environment().skybox_cubemap)},
            {"skyboxIntensity", scene.environment().skybox_intensity},
            {"skyboxRotationDegrees", scene.environment().skybox_rotation_degrees}
        };
        root["entities"] = json::array();

        for (const Entity& root_entity : scene.getRootEntities())
            writeEntityDFS(scene, root_entity.GetHandle(), root["entities"], registry);

        std::ofstream ofs(path, std::ios::out | std::ios::trunc);
        if (!ofs.is_open())
            return false;

        ofs << root.dump(4);
        return true;
    }

    bool SceneSerializer::DeserializeFromFile(Scene& scene, const std::filesystem::path& path, const AssetRegistry* registry)
    {
        std::ifstream ifs(path);
        if (!ifs.is_open())
            return false;

        json root;
        try { ifs >> root; }
        catch (...) { return false; }

        const int version = root.value("meta", json::object()).value("version", 0);
        // 兼容：允许 1/2
        if (version != 1 && version != 2)
            return false;

        if (!root.contains("entities") || !root["entities"].is_array())
            return false;

        if (root.contains("environment") && root["environment"].is_object())
        {
            const auto& env = root["environment"];
            scene.environment().skybox_cubemap =
                resolveAsset(env, "skyboxCubemap", "skyboxCubemapPath", registry);
            scene.environment().skybox_intensity = env.value("skyboxIntensity", 1.0f);
            scene.environment().skybox_rotation_degrees = env.value("skyboxRotationDegrees", 0.0f);
        }

        // Phase A: create entities + set TRS + read components (v2)
        struct PendingRel { uint64_t self = 0; uint64_t parent = 0; };
        std::vector<PendingRel> pending;
        pending.reserve(root["entities"].size());

        for (const auto& je : root["entities"])
        {
            const uint64_t uuid = je.value("uuid", 0ull);
            if (uuid == 0)
                continue;

            const std::string name = je.value("name", std::string("Entity"));
            const uint64_t parent_uuid = je.value("parent", 0ull);

            Entity e = scene.createEntityWithUUID(UUID{ uuid }, name);

            // Transform
            if (je.contains("transform") && je["transform"].is_object())
            {
                const auto& jt = je["transform"];
                auto& tc = e.GetComponent<TransformComponent>();

                if (jt.contains("t")) tc.Position = vec3From(jt["t"]);
                if (jt.contains("r")) tc.Rotation = quatFrom(jt["r"]);
                if (jt.contains("s")) tc.Scale = vec3From(jt["s"]);

                // 缓存标记让系统重算矩阵
                tc.DirtyLocal = true;
                tc.DirtyWorld = true;
            }

            // v2：读取组件；v1：这些字段不存在会被忽略
            readComponents(scene.getRegistry(), e.GetHandle(), je, registry);

            pending.push_back(PendingRel{ uuid, parent_uuid });
        }

        // Phase B: set parent relations (keep local)
        std::unordered_map<uint64_t, std::vector<uint64_t>> children_by_parent;
        children_by_parent.reserve(pending.size());
        for (const auto& rel : pending)
        {
            if (rel.parent != 0)
                children_by_parent[rel.parent].push_back(rel.self);
        }

        for (const auto& [parent_uuid, child_uuids] : children_by_parent)
        {
            Entity parent = scene.findEntityByUUID(UUID{ parent_uuid });
            if (!parent.IsValid())
                continue;

            for (auto it = child_uuids.rbegin(); it != child_uuids.rend(); ++it)
            {
                Entity child = scene.findEntityByUUID(UUID{ *it });
                if (!child.IsValid())
                    continue;

                scene.SetParent(child, parent, /*worldPositionStays=*/false);
            }
        }

        scene.onUpdate(0.0f);
        return true;
    }
} // namespace Hybrid
