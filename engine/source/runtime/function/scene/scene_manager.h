#pragma once
#include <memory>

namespace Hybrid
{
    class Scene;

    class SceneManager
    {
    public:
        void SetActiveScene(std::shared_ptr<Scene> scene) { m_ActiveScene = std::move(scene); }
        std::shared_ptr<Scene> GetActiveScene() const { return m_ActiveScene; }

        bool HasActiveScene() const { return static_cast<bool>(m_ActiveScene); }

    private:
        std::shared_ptr<Scene> m_ActiveScene;
    };
}
