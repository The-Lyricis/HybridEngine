#pragma once

#include "runtime/modules/physics/collision/collision_types.h"

namespace Hybrid
{
    class Scene;
    struct TransformComponent;
    struct ColliderComponent;

    class PhysicsSystem
    {
    public:
        void initialize();
        void shutdown();

        void tick(float dt, Scene& scene);

        bool isInitialized() const { return m_Initialized; }

    private:
        void stepSimulation(float dt, Scene& scene);

        void integrateRigidbodies(float dt, Scene& scene);
        void solveCollisions(Scene& scene);

        AABB buildAABB(const TransformComponent& transform, const ColliderComponent& collider) const;

    private:
        bool m_Initialized = false;
    };
}
