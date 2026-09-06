#include "editor_session_controller.h"

#include "runtime/modules/scene/scene.h"
#include "runtime/runtime/engine.h"

namespace Hybrid
{
    EditorSessionController::EditorSessionController(HybridEngine& engine)
        : m_engine(engine), m_editor_scene(engine.getActiveScene())
    {
        m_engine.setFixedUpdateEnabled(false);
        m_engine.setSceneUpdateEnabled(true);
    }

    bool EditorSessionController::setEditorScene(std::shared_ptr<Scene> scene)
    {
        if (!scene)
            return false;
        if (m_playing)
            exitPlayMode();
        m_editor_scene = std::move(scene);
        m_engine.setFixedUpdateEnabled(false);
        m_engine.setSceneUpdateEnabled(true);
        return m_engine.setActiveScene(m_editor_scene);
    }

    bool EditorSessionController::enterPlayModeFromScene(const std::shared_ptr<Scene>& source_scene)
    {
        if (m_playing || !source_scene)
            return false;
        m_editor_scene = source_scene;
        m_runtime_scene = source_scene->cloneRuntime();
        if (!m_runtime_scene || !m_engine.setActiveScene(m_runtime_scene))
        {
            m_runtime_scene.reset();
            return false;
        }
        m_playing = true;
        m_paused = false;
        m_engine.setFixedUpdateEnabled(true);
        m_engine.setSceneUpdateEnabled(true);
        return true;
    }

    void EditorSessionController::exitPlayMode()
    {
        if (!m_playing)
            return;
        m_playing = false;
        m_paused = false;
        m_runtime_scene.reset();
        m_engine.setFixedUpdateEnabled(false);
        m_engine.setSceneUpdateEnabled(true);
        if (m_editor_scene)
            (void)m_engine.setActiveScene(m_editor_scene);
    }

    void EditorSessionController::togglePause()
    {
        if (!m_playing)
            return;
        m_paused = !m_paused;
        m_engine.setSceneUpdateEnabled(!m_paused);
    }
} // namespace Hybrid
