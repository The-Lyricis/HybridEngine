#pragma once
#include <memory>

namespace Hybrid
{
    class Scene;

    class SceneManager
    {
    public:
        void setActiveScene(std::shared_ptr<Scene> scene) { m_ActiveScene = std::move(scene); }
        std::shared_ptr<Scene> getActiveScene() const { return m_ActiveScene; }

        bool hasActiveScene() const { return static_cast<bool>(m_ActiveScene); }

    private:
        std::shared_ptr<Scene> m_ActiveScene;
    };
}
