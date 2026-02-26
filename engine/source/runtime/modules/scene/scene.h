#pragma once
#include <entt/entt.hpp>
#include <string>

#include "entity.h"
#include "components.h"

namespace Hybrid
{
    class Scene
    {
    public:
        Scene() = default;
        ~Scene() = default;

        Entity createEntity(const std::string& name = "Entity");
        Entity createCameraEntity(const std::string& name = "Camera", bool primary = true);
        Entity createRenderableEntity(const std::string& name = "Renderable");

        bool SetParent(Entity child, Entity parent, bool worldPositionStays = true);
        bool Detach(Entity child, bool worldPositionStays = true);
        bool IsDescendant(Entity node, Entity ancestor) const;
        void MarkDirtyRecursive(Entity root);
        void DestroyEntityRecursive(Entity e);

        void destroyEntity(Entity entity);

        void onUpdate(float dt);

        // 给 Entity 和外部系统访问
        entt::registry& getRegistry() { return m_Registry; }
        const entt::registry& getRegistry() const { return m_Registry; }

    private:
        void updateTransformHierarchy();

    private:
        entt::registry m_Registry;

        // 简易 ID 生成（后续换 UUID 系统）
        uint64_t m_NextID = 1;
    };
}
