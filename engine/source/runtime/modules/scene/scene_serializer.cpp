#include "scene_serializer.h"

#include <fstream>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>

#include "scene.h"
#include "components.h"
#include "uuid.h"

namespace Hybrid
{
    namespace
    {
        using json = nlohmann::json;

        // -----------------------------
        // glm helpers (vec3 / quat)
        // quat 约定存 [w, x, y, z]，与你当前 glm::quat 初始化一致
        // -----------------------------
        json toJson(const glm::vec3& v) { return json::array({ v.x, v.y, v.z }); }
        json toJson(const glm::quat& q) { return json::array({ q.w, q.x, q.y, q.z }); }

        glm::vec3 vec3From(const json& j)
        {
            return glm::vec3(j.at(0).get<float>(), j.at(1).get<float>(), j.at(2).get<float>());
        }

        glm::quat quatFrom(const json& j)
        {
            return glm::quat(j.at(0).get<float>(), j.at(1).get<float>(), j.at(2).get<float>(), j.at(3).get<float>());
        }

        // -----------------------------
        // entity info extraction
        // -----------------------------
        uint64_t getUUID(entt::registry& reg, entt::entity e)
        {
            if (!reg.valid(e) || !reg.all_of<IDComponent>(e))
                return 0;
            return static_cast<uint64_t>(reg.get<IDComponent>(e).ID);
        }

        std::string getName(entt::registry& reg, entt::entity e)
        {
            if (!reg.valid(e) || !reg.all_of<TagComponent>(e))
                return "Entity";
            return reg.get<TagComponent>(e).Tag;
        }

        // -----------------------------
        // DFS write in hierarchy order (root -> children)
        // -----------------------------
        void writeEntityDFS(const Scene& scene, entt::entity e, json& outEntities)
        {
            auto& reg = const_cast<entt::registry&>(scene.getRegistry());

            // 基础组件必须存在（如果未来允许某些内部实体缺失，可在这里容错）
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

            outEntities.push_back(std::move(je));

            // children in sibling order
            entt::entity child = tc.FirstChild;
            while (child != entt::null)
            {
                entt::entity next = entt::null;
                if (reg.valid(child) && reg.all_of<TransformComponent>(child))
                    next = reg.get<TransformComponent>(child).NextSibling;

                writeEntityDFS(scene, child, outEntities);
                child = next;
            }
        }

        // root detection: tc.Parent == null
        std::vector<entt::entity> collectRoots(const Scene& scene)
        {
            std::vector<entt::entity> roots;
            auto& reg = const_cast<entt::registry&>(scene.getRegistry());

            auto view = reg.view<TransformComponent>();
            for (auto e : view)
            {
                const auto& tc = view.get<TransformComponent>(e);
                if (tc.Parent == entt::null)
                    roots.push_back(e);
            }
            return roots;
        }
    } // namespace

    bool SceneSerializer::SerializeToFile(const Scene& scene, const std::filesystem::path& path)
    {
        // 确保 transform 层级缓存更新（避免 parent/world 脏状态影响导出）
        // 这里不修改逻辑数据，只是同步矩阵和脏标记
        const_cast<Scene&>(scene).onUpdate(0.0f);

        json root;
        root["meta"] = {
            {"version", 1}
        };
        root["entities"] = json::array();

        // 按层级顺序输出：root DFS
        for (auto r : collectRoots(scene))
            writeEntityDFS(scene, r, root["entities"]);

        std::ofstream ofs(path, std::ios::out | std::ios::trunc);
        if (!ofs.is_open())
            return false;

        // 4-space pretty print，便于版本控制 diff；后续可加开关
        ofs << root.dump(4);
        return true;
    }

    bool SceneSerializer::DeserializeFromFile(Scene& scene, const std::filesystem::path& path)
    {
        std::ifstream ifs(path);
        if (!ifs.is_open())
            return false;

        json root;
        try
        {
            ifs >> root;
        }
        catch (...)
        {
            return false;
        }

        // 版本检查（v0：只支持 version=1）
        const int version = root.value("meta", json::object()).value("version", 0);
        if (version != 1)
            return false;

        if (!root.contains("entities") || !root["entities"].is_array())
            return false;

        auto& reg = scene.getRegistry();

        // -----------------------------
        // Phase A: create all entities + set local TRS
        // -----------------------------
        struct PendingRel
        {
            uint64_t self = 0;
            uint64_t parent = 0;
        };
        std::vector<PendingRel> pending;
        pending.reserve(root["entities"].size());

        for (const auto& je : root["entities"])
        {
            const uint64_t uuid = je.value("uuid", 0ull);
            if (uuid == 0)
                continue;

            const std::string name = je.value("name", std::string("Entity"));
            const uint64_t parent_uuid = je.value("parent", 0ull);

            // 用文件 UUID 创建
            Entity e = scene.createEntityWithUUID(UUID{ uuid }, name);

            // 设置 TRS（local）
            if (je.contains("transform"))
            {
                const auto& jt = je["transform"];
                auto& tc = e.GetComponent<TransformComponent>();

                if (jt.contains("t")) tc.Position = vec3From(jt["t"]);
                if (jt.contains("r")) tc.Rotation = quatFrom(jt["r"]);
                if (jt.contains("s")) tc.Scale = vec3From(jt["s"]);

                // 让你的 transform 系统重新计算矩阵
                tc.DirtyLocal = true;
                tc.DirtyWorld = true;

                // 层级链接此时保持 null，Phase B 再设
                tc.Parent = entt::null;
                tc.FirstChild = entt::null;
                tc.NextSibling = entt::null;
                tc.PrevSibling = entt::null;
            }

            pending.push_back(PendingRel{ uuid, parent_uuid });
        }

        // -----------------------------
        // Phase B: setup parent relationships
        // 注意：保持 local 不变，所以 worldPositionStays = false
        // -----------------------------
        for (const auto& rel : pending)
        {
            if (rel.parent == 0)
                continue;

            Entity child = scene.findEntityByUUID(UUID{ rel.self });
            Entity parent = scene.findEntityByUUID(UUID{ rel.parent });
            if (!child.IsValid() || !parent.IsValid())
                continue;

            scene.SetParent(child, parent, /*worldPositionStays=*/false);
        }

        // 最后更新一次 hierarchy/world matrix
        scene.onUpdate(0.0f);
        return true;
    }

} // namespace Hybrid
