#include "runtime/modules/physics/physics_system.h"
#include "runtime/modules/scene/scene.h"
#include <runtime/core/base/macro.h>
#include "runtime/modules/scene/components.h"
#include "runtime/modules/scene/components/collider_component.h"
#include "runtime/modules/scene/components/rigidbody_component.h"
#include "runtime/modules/physics/collision/collision_types.h"

namespace Hybrid
{
    namespace
    {
        constexpr const char* kPhysicsSystemLogTag = "[PhysicsSystem]";
    } // namespace

    void PhysicsSystem::initialize()
    {
        if (m_Initialized)
        {
            HBD_CORE_WARN("{} initialize_skipped reason=already_initialized", kPhysicsSystemLogTag);
            return;
        }

        m_Initialized = true;
        HBD_CORE_INFO("{} initialize_completed", kPhysicsSystemLogTag);
    }

    void PhysicsSystem::shutdown()
    {
        if (!m_Initialized)
        {
            HBD_CORE_WARN("{} shutdown_skipped reason=not_initialized", kPhysicsSystemLogTag);
            return;
        }

        m_Initialized = false;
        HBD_CORE_INFO("{} shutdown_completed", kPhysicsSystemLogTag);
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
            if (!rigidbody.Enabled)
                continue;
            if (rigidbody.IsKinematic)
                continue;

            glm::vec3 total_force = rigidbody.Force + rigidbody.ConstantForce;
            if (rigidbody.UseGravity)
            {
                total_force += glm::vec3(0.0f, -9.8f * rigidbody.Mass, 0.0f);
            }
            const glm::vec3 acceleration = rigidbody.Mass > 0.0f
                ? total_force / rigidbody.Mass
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

    void PhysicsSystem::solveCollisions(Scene& scene)
    {
        auto& registry = scene.getRegistry();

        auto dynamicView = registry.view<TransformComponent, RigidbodyComponent, ColliderComponent>();
        auto staticView = registry.view<TransformComponent, ColliderComponent>();

        constexpr float kCollisionSlop = 0.001f;

        for (auto dynamicEntity : dynamicView)
        {
            auto& dynamicTransform = dynamicView.get<TransformComponent>(dynamicEntity);
            auto& dynamicBody = dynamicView.get<RigidbodyComponent>(dynamicEntity);
            auto& dynamicCollider = dynamicView.get<ColliderComponent>(dynamicEntity);

            if (!dynamicBody.Enabled)
                continue;
            if (!dynamicCollider.Enabled || dynamicCollider.Type != ColliderType::Box)
                continue;

            AABB dynamicAABB = buildAABB(dynamicTransform, dynamicCollider);

            for (auto staticEntity : staticView)
            {
                if (dynamicEntity == staticEntity)
                    continue;

                if (registry.all_of<RigidbodyComponent>(staticEntity))
                {
                    const auto& staticBody = registry.get<RigidbodyComponent>(staticEntity);
                    if (staticBody.Enabled)
                        continue;
                }

                auto& staticTransform = staticView.get<TransformComponent>(staticEntity);
                auto& staticCollider = staticView.get<ColliderComponent>(staticEntity);

                if (!staticCollider.Enabled || staticCollider.Type != ColliderType::Box)
                    continue;

                const AABB staticAABB = buildAABB(staticTransform, staticCollider);
                const CollisionHit hit = Hybrid::IntersectAABB(dynamicAABB, staticAABB);

                if (!hit.Hit)
                    continue;

                const float correction = hit.Penetration + kCollisionSlop;
                dynamicTransform.Position -= hit.Normal * correction;
                dynamicTransform.DirtyLocal = true;
                dynamicTransform.DirtyWorld = true;

                const float normal_velocity = glm::dot(dynamicBody.Velocity, hit.Normal);
                if (normal_velocity > 0.0f)
                    dynamicBody.Velocity -= hit.Normal * normal_velocity;

                dynamicAABB = buildAABB(dynamicTransform, dynamicCollider);
            }
        }
    }
}
