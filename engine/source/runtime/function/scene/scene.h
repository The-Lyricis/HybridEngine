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

        void destroyEntity(Entity entity);

        void onUpdate(float dt);

        // 给 Entity 和外部系统访问
        entt::registry& getRegistry() { return m_Registry; }
        const entt::registry& getRegistry() const { return m_Registry; }

    private:
        entt::registry m_Registry;

        // 简易 ID 生成（后续换 UUID 系统）
        uint64_t m_NextID = 1;
    };
}
