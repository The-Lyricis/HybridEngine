// scene_serializer.cpp
#include "scene_serializer.h"

#include <algorithm>
#include <fstream>
#include <vector>
#include <nlohmann/json.hpp>

#include "runtime/modules/asset/asset_registry.h"
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

        static void writeComponents(entt::registry& reg, entt::entity e, json& je, const AssetRegistry* registry)
        {
            // Camera
            if (reg.all_of<CameraComponent>(e))
            {
                const auto& c = reg.get<CameraComponent>(e);
                je["camera"] = {
                    {"primary", c.Primary},
                    {"fovY", c.FovY},
                    {"near", c.Near},
                    {"far",  c.Far},
                    {"clearMode", static_cast<int>(c.ClearMode)},
                    {"clearColor", toJson(c.ClearColor)}
                };
            }

            // MeshRenderer
            if (reg.all_of<MeshRendererComponent>(e))
            {
                const auto& mr = reg.get<MeshRendererComponent>(e);
                je["meshRenderer"] = {
                    {"mesh", mr.Mesh.value},
                    {"material", mr.Material.value},
                    {"tint", toJson(mr.Tint)}
                };

                const std::string mesh_path = assetPathFor(registry, mr.Mesh);
                if (!mesh_path.empty())
                    je["meshRenderer"]["meshPath"] = mesh_path;

                const std::string material_path = assetPathFor(registry, mr.Material);
                if (!material_path.empty())
                    je["meshRenderer"]["materialPath"] = material_path;
            }

            // DirectionalLight
            if (reg.all_of<DirectionalLightComponent>(e))
            {
                const auto& dl = reg.get<DirectionalLightComponent>(e);
                je["dirLight"] = {
                    {"color", toJson(dl.Color)},
                    {"intensity", dl.Intensity}
                };
            }

            // PointLight
            if (reg.all_of<PointLightComponent>(e))
            {
                const auto& pl = reg.get<PointLightComponent>(e);
                je["pointLight"] = {
                    {"color", toJson(pl.Color)},
                    {"intensity", pl.Intensity},
                    {"range", pl.Range}
                };
            }

            if (reg.all_of<ColliderComponent>(e))
            {
                const auto& collider = reg.get<ColliderComponent>(e);
                json jc = {
                    {"type", static_cast<int>(collider.Type)},
                    {"enabled", collider.Enabled},
                    {"isTrigger", collider.IsTrigger},
                    {"center", toJson(collider.Center)}
                };

                if (collider.Type == ColliderType::Box)
                    jc["halfExtents"] = toJson(collider.Box.HalfExtents);
                else if (collider.Type == ColliderType::Sphere)
                    jc["radius"] = collider.Sphere.Radius;

                je["collider"] = std::move(jc);
            }

            if (reg.all_of<RigidbodyComponent>(e))
            {
                const auto& rb = reg.get<RigidbodyComponent>(e);
                je["rigidbody"] = {
                    {"velocity", toJson(rb.Velocity)},
                    {"force", toJson(rb.Force)},
                    {"mass", rb.Mass},
                    {"useGravity", rb.UseGravity},
                    {"isKinematic", rb.IsKinematic}
                };
            }
        }

        static void readComponents(entt::registry& reg, entt::entity e, const json& je, const AssetRegistry* registry)
        {
            // Camera
            if (je.contains("camera") && je["camera"].is_object())
            {
                const auto& jc = je["camera"];
                auto& c = reg.all_of<CameraComponent>(e) ? reg.get<CameraComponent>(e) : reg.emplace<CameraComponent>(e);
                c.Primary = jc.value("primary", false);
                c.FovY = jc.value("fovY", 45.0f);
                c.Near = jc.value("near", 0.1f);
                c.Far = jc.value("far", 1000.0f);
                c.ClearMode = static_cast<CameraClearMode>(
                    jc.value("clearMode", static_cast<int>(CameraClearMode::SolidColor)));
                if (jc.contains("clearColor") && jc["clearColor"].is_array() && jc["clearColor"].size() == 4)
                    c.ClearColor = vec4From(jc["clearColor"]);
            }

            // MeshRenderer
            if (je.contains("meshRenderer") && je["meshRenderer"].is_object())
            {
                const auto& jm = je["meshRenderer"];
                auto& mr = reg.all_of<MeshRendererComponent>(e) ? reg.get<MeshRendererComponent>(e)
                    : reg.emplace<MeshRendererComponent>(e);
                mr.Mesh = resolveAsset(jm, "mesh", "meshPath", registry);
                mr.Material = resolveAsset(jm, "material", "materialPath", registry);

                if (jm.contains("tint") && jm["tint"].is_array() && jm["tint"].size() == 4)
                    mr.Tint = vec4From(jm["tint"]);
            }

            // DirectionalLight
            if (je.contains("dirLight") && je["dirLight"].is_object())
            {
                const auto& jl = je["dirLight"];
                auto& dl = reg.all_of<DirectionalLightComponent>(e) ? reg.get<DirectionalLightComponent>(e)
                    : reg.emplace<DirectionalLightComponent>(e);
                if (jl.contains("color") && jl["color"].is_array() && jl["color"].size() == 3)
                    dl.Color = vec3From(jl["color"]);
                dl.Intensity = jl.value("intensity", 1.0f);
            }

            // PointLight
            if (je.contains("pointLight") && je["pointLight"].is_object())
            {
                const auto& jl = je["pointLight"];
                auto& pl = reg.all_of<PointLightComponent>(e) ? reg.get<PointLightComponent>(e)
                    : reg.emplace<PointLightComponent>(e);
                if (jl.contains("color") && jl["color"].is_array() && jl["color"].size() == 3)
                    pl.Color = vec3From(jl["color"]);
                pl.Intensity = jl.value("intensity", 1.0f);
                pl.Range = jl.value("range", 10.0f);
            }

            if (je.contains("collider") && je["collider"].is_object())
            {
                const auto& jc = je["collider"];
                auto& collider = reg.all_of<ColliderComponent>(e) ? reg.get<ColliderComponent>(e)
                    : reg.emplace<ColliderComponent>(e);

                collider.Type = static_cast<ColliderType>(jc.value("type", static_cast<int>(ColliderType::Box)));
                collider.Enabled = jc.value("enabled", true);
                collider.IsTrigger = jc.value("isTrigger", false);

                if (jc.contains("center") && jc["center"].is_array() && jc["center"].size() == 3)
                    collider.Center = vec3From(jc["center"]);

                if (jc.contains("halfExtents") && jc["halfExtents"].is_array() && jc["halfExtents"].size() == 3)
                    collider.Box.HalfExtents = vec3From(jc["halfExtents"]);

                collider.Box.HalfExtents.x = std::max(0.0f, collider.Box.HalfExtents.x);
                collider.Box.HalfExtents.y = std::max(0.0f, collider.Box.HalfExtents.y);
                collider.Box.HalfExtents.z = std::max(0.0f, collider.Box.HalfExtents.z);

                collider.Sphere.Radius = std::max(0.0f, jc.value("radius", collider.Sphere.Radius));
            }

            if (je.contains("rigidbody") && je["rigidbody"].is_object())
            {
                const auto& jr = je["rigidbody"];
                auto& rb = reg.all_of<RigidbodyComponent>(e) ? reg.get<RigidbodyComponent>(e)
                    : reg.emplace<RigidbodyComponent>(e);

                if (jr.contains("velocity") && jr["velocity"].is_array() && jr["velocity"].size() == 3)
                    rb.Velocity = vec3From(jr["velocity"]);
                if (jr.contains("force") && jr["force"].is_array() && jr["force"].size() == 3)
                    rb.Force = vec3From(jr["force"]);

                rb.Mass = std::max(0.0f, jr.value("mass", 1.0f));
                rb.UseGravity = jr.value("useGravity", true);
                rb.IsKinematic = jr.value("isKinematic", false);
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
