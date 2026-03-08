#pragma once

namespace Hybrid
{
    class Scene;

    class PhysicsSystem
    {
    public:
        void initialize();
        void shutdown();

        void tick(float dt, Scene& scene);

        bool isInitialized() const { return m_Initialized; }

    private:
        void stepSimulation(float dt, Scene& scene);

    private:
        bool m_Initialized = false;
    };
}
