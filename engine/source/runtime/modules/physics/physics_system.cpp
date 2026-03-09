#include "runtime/modules/physics/physics_system.h"
#include "runtime/modules/scene/scene.h"
#include <runtime/core/base/macro.h>
#include "runtime/modules/scene/components.h"
#include "runtime/modules/physics/components/collider_component.h"
#include "runtime/modules/physics/components/rigidbody_component.h"
#include "runtime/modules/physics/collision/collision_types.h"

namespace Hybrid
{
    void PhysicsSystem::initialize()
    {
        if (m_Initialized)
        {
            HBD_CORE_WARN("PhysicsSystem already initialized.");
            return;
        }

        m_Initialized = true;
        HBD_CORE_INFO("PhysicsSystem initialized.");
    }

    void PhysicsSystem::shutdown()
    {
        if (!m_Initialized)
        {
            HBD_CORE_WARN("PhysicsSystem shutdown called before initialization.");
            return;
        }

        m_Initialized = false;
        HBD_CORE_INFO("PhysicsSystem shutdown.");
    }

    void PhysicsSystem::tick(float dt, Scene& scene)
    {
        if (!m_Initialized)
            return;

        stepSimulation(dt, scene);
    }

    void PhysicsSystem::stepSimulation(float dt, Scene& scene)
    {
        integrateRigidbodies(dt, scene);
        solveCollisions(scene);
    }

    void PhysicsSystem::integrateRigidbodies(float dt, Scene& scene)
    {
        auto& registry = scene.getRegistry();
        auto view = registry.view<TransformComponent, RigidbodyComponent>();
        for (auto entity : view)
        {
            auto& transform = view.get<TransformComponent>(entity);
            auto& rigidbody = view.get<RigidbodyComponent>(entity);
            if (rigidbody.IsKinematic)
                continue;
            if (rigidbody.UseGravity)
            {
                rigidbody.Force += glm::vec3(0.0f, -9.8f * rigidbody.Mass, 0.0f);
            }
            const glm::vec3 acceleration = rigidbody.Mass > 0.0f
                ? rigidbody.Force / rigidbody.Mass
                : glm::vec3(0.0f);
            rigidbody.Velocity += acceleration * dt;
            transform.Position += rigidbody.Velocity * dt;
            transform.DirtyLocal = true;
            transform.DirtyWorld = true;
            rigidbody.Force = glm::vec3(0.0f);
        }
    }

    AABB PhysicsSystem::buildAABB(const TransformComponent& transform, const ColliderComponent& collider) const
    {
        AABB aabb{};

        const glm::vec3 worldCenter = transform.Position + collider.Center;
        const glm::vec3 absScale = glm::abs(transform.Scale);
        const glm::vec3 worldHalfExtents = collider.Box.HalfExtents * absScale;

        aabb.Min = worldCenter - worldHalfExtents;
        aabb.Max = worldCenter + worldHalfExtents;

        return aabb;
    }

    CollisionHit PhysicsSystem::intersectAABB(const AABB& a, const AABB& b) const
    {
        CollisionHit hit{};

        const float overlapX = std::min(a.Max.x, b.Max.x) - std::max(a.Min.x, b.Min.x);
        if (overlapX <= 0.0f)
            return hit;

        const float overlapY = std::min(a.Max.y, b.Max.y) - std::max(a.Min.y, b.Min.y);
        if (overlapY <= 0.0f)
            return hit;

        const float overlapZ = std::min(a.Max.z, b.Max.z) - std::max(a.Min.z, b.Min.z);
        if (overlapZ <= 0.0f)
            return hit;

        hit.Hit = true;

        const glm::vec3 centerA = 0.5f * (a.Min + a.Max);
        const glm::vec3 centerB = 0.5f * (b.Min + b.Max);
        const glm::vec3 delta = centerB - centerA;

        hit.Penetration = overlapX;
        hit.Normal = glm::vec3((delta.x >= 0.0f) ? 1.0f : -1.0f, 0.0f, 0.0f);

        if (overlapY < hit.Penetration)
        {
            hit.Penetration = overlapY;
            hit.Normal = glm::vec3(0.0f, (delta.y >= 0.0f) ? 1.0f : -1.0f, 0.0f);
        }

        if (overlapZ < hit.Penetration)
        {
            hit.Penetration = overlapZ;
            hit.Normal = glm::vec3(0.0f, 0.0f, (delta.z >= 0.0f) ? 1.0f : -1.0f);
        }

        return hit;
    }
    void PhysicsSystem::solveCollisions(Scene& scene)
    {
        auto& registry = scene.getRegistry();

        auto dynamicView = registry.view<TransformComponent, RigidbodyComponent, ColliderComponent>();
        auto staticView = registry.view<TransformComponent, ColliderComponent>();

        for (auto dynamicEntity : dynamicView)
        {
            auto& dynamicTransform = dynamicView.get<TransformComponent>(dynamicEntity);
            auto& dynamicBody = dynamicView.get<RigidbodyComponent>(dynamicEntity);
            auto& dynamicCollider = dynamicView.get<ColliderComponent>(dynamicEntity);

            if (!dynamicCollider.Enabled || dynamicCollider.Type != ColliderType::Box)
                continue;

            const AABB dynamicAABB = buildAABB(dynamicTransform, dynamicCollider);

            for (auto staticEntity : staticView)
            {
                if (dynamicEntity == staticEntity)
                    continue;

                if (registry.all_of<RigidbodyComponent>(staticEntity))
                    continue;

                auto& staticTransform = staticView.get<TransformComponent>(staticEntity);
                auto& staticCollider = staticView.get<ColliderComponent>(staticEntity);

                if (!staticCollider.Enabled || staticCollider.Type != ColliderType::Box)
                    continue;

                const AABB staticAABB = buildAABB(staticTransform, staticCollider);
                const CollisionHit hit = intersectAABB(dynamicAABB, staticAABB);

                if (hit.Hit)
                {
                    HBD_CORE_TRACE("Collision detected. Penetration = {}", hit.Penetration);
                }
            }
        }
    }
}
