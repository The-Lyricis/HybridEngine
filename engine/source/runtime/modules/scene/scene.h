#pragma once
#include <entt/entt.hpp>
#include <string>
#include <unordered_map>
#include <vector>

#include "entity.h"
#include "components.h"
#include "uuid.h"

namespace Hybrid
{
    class Scene
    {
    public:
        Scene() = default;
        ~Scene() = default;

        // 普通创建：用于编辑器交互（自动生成 UUID）
        Entity createEntity(const std::string& name = "Entity");

        // 反序列化创建：用于 load 时“按文件 UUID”创建
        Entity createEntityWithUUID(UUID id, const std::string& name = "Entity");

        Entity createCameraEntity(const std::string& name = "Camera", bool primary = true);
        Entity createRenderableEntity(const std::string& name = "Renderable");

        // 查找：序列化/反序列化/引用都需要
        Entity findEntityByUUID(UUID id) const;

        // 关系操作
        bool SetParent(Entity child, Entity parent, bool worldPositionStays = true);
        bool Detach(Entity child, bool worldPositionStays = true);

        // 语义更明确：是否为“严格后代”（不包含自己）
        bool IsDescendant(Entity node, Entity ancestor) const;

        void MarkDirtyRecursive(Entity root);
        void DestroyEntityRecursive(Entity e);
        void destroyEntity(Entity entity);

        void onUpdate(float dt);

        // 便于序列化：稳定遍历入口（按 root 开始 DFS/层级输出）
        std::vector<Entity> getRootEntities() const;

        // 给 Entity 和外部系统访问
        entt::registry& getRegistry() { return m_Registry; }
        const entt::registry& getRegistry() const { return m_Registry; }

    private:
        void updateTransformHierarchy();

        void onEntityCreated(Entity e);
        void onEntityDestroyed(entt::entity h);

        // 断开父子链路统一入口（修复悬空 firstChild 等问题）
        void detachFromParentLinks(entt::entity child);
        void attachToParentLinks(entt::entity child, entt::entity parent);

    private:
        entt::registry m_Registry;

        // UUID -> entt::entity（用于加载、引用定位）
        std::unordered_map<uint64_t, entt::entity> m_EntityMap;
    };
} // namespace Hybrid
