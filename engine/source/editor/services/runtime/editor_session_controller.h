#pragma once

#include <memory>

namespace Hybrid
{
    class HybridEngine;
    class Scene;

    // Owns editor-only document/runtime clone and play/pause state.
    class EditorSessionController
    {
    public:
        explicit EditorSessionController(HybridEngine& engine);

        bool setEditorScene(std::shared_ptr<Scene> scene);
        bool enterPlayModeFromScene(const std::shared_ptr<Scene>& source_scene);
        void exitPlayMode();
        void togglePause();

        bool isPlayMode() const { return m_playing; }
        bool isPaused() const { return m_paused; }
        std::shared_ptr<Scene> editorScene() const { return m_editor_scene; }
        std::shared_ptr<Scene> runtimeScene() const { return m_runtime_scene; }

    private:
        HybridEngine& m_engine;
        std::shared_ptr<Scene> m_editor_scene;
        std::shared_ptr<Scene> m_runtime_scene;
        bool m_playing = false;
        bool m_paused = false;
    };
} // namespace Hybrid
