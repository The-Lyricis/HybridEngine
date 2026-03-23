#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <entt/entt.hpp>

#include "components.h"
#include "entity.h"
#include "runtime/modules/asset/asset_type.h"
#include "uuid.h"

namespace Hybrid
{
    struct SceneEnvironmentSettings
    {
        AssetID skybox_cubemap{};
        float skybox_intensity = 1.0f;
        float skybox_rotation_degrees = 0.0f;
    };

    class Scene
    {
    public:
        Scene() = default;
        ~Scene() = default;

        Entity createEntity(const std::string& name = "Entity");
        Entity createEntityWithUUID(UUID id, const std::string& name = "Entity");

        Entity createCameraEntity(const std::string& name = "Camera", bool primary = true);
        Entity createRenderableEntity(const std::string& name = "Renderable");

        Entity findEntityByUUID(UUID id) const;

        bool SetParent(Entity child, Entity parent, bool worldPositionStays = true);
        bool Detach(Entity child, bool worldPositionStays = true);
        bool IsDescendant(Entity node, Entity ancestor) const;

        void MarkDirtyRecursive(Entity root);
        void DestroyEntityRecursive(Entity e);
        void destroyEntity(Entity entity);

        void onUpdate(float dt);
        std::shared_ptr<Scene> cloneRuntime() const;
        std::vector<Entity> getRootEntities() const;

        entt::registry& getRegistry() { return m_Registry; }
        const entt::registry& getRegistry() const { return m_Registry; }

        void setName(std::string name) { m_Name = std::move(name); }
        const std::string& getName() const { return m_Name; }

        SceneEnvironmentSettings& environment() { return m_Environment; }
        const SceneEnvironmentSettings& environment() const { return m_Environment; }

    private:
        void updateTransformHierarchy();
        entt::entity getLastRoot() const;

        void onEntityCreated(Entity e);
        void onEntityDestroyed(entt::entity h);
        void detachFromParentLinks(entt::entity child);
        void attachToParentLinks(entt::entity child, entt::entity parent);

    private:
        entt::registry m_Registry;
        entt::entity m_FirstRoot{entt::null};
        std::unordered_map<uint64_t, entt::entity> m_EntityMap;
        std::string m_Name = "Untitled";
        SceneEnvironmentSettings m_Environment{};
    };
} // namespace Hybrid
