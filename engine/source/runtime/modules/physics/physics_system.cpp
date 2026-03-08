#include "runtime/modules/physics/physics_system.h"
#include "runtime/modules/scene/scene.h"
#include <runtime/core/base/macro.h>

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
}
