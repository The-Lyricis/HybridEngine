#pragma once

#include <cstdint>
#include <memory>

#include "runtime/core/event/application_event.h"
#include "runtime/core/event/input_event.h"
#include "runtime/core/event/layer.h"
#include "runtime/modules/asset/runtime_resource_system.h"
#include "runtime/modules/input/input_layer.h"
#include "runtime/modules/render/runtime/editor_render_ext.h"
#include "runtime/modules/render/runtime/frame_context.h"
#include "runtime/modules/render/public/graphics_context.h"
#include "runtime/modules/render/runtime/render_flags.h"
#include "runtime/modules/render/runtime/render_system.h"
#include "runtime/modules/scene/scene.h"
#include "runtime/modules/scene/scene_manager.h"
#include "runtime/modules/window/window_system.h"
#include "runtime/modules/physics/physics_system.h"
#include "runtime/modules/physics/components/rigidbody_component.h"

namespace Hybrid
{
    // Runtime application host. Editor extends behavior via layers/services.
    class HybridEngine
    {
        enum class SceneRunState
        {
            Edit = 0,
            Play
        };

    public:
        void initialize(); // Create window, graphics context, scene and runtime systems.
        void run();        // Main loop: poll -> begin-frame -> update -> render -> UI -> end-frame -> present.
        void shutdown();   // Tear down layers and core systems.

        void onEvent(Event& e);          // Dispatch platform input/window events.
        void pushLayer(Layer* layer);    // Insert gameplay/editor layer.
        void pushOverlay(Layer* layer);  // Insert overlay layer (e.g. ImGui).

        WindowSystem& getWindowSystem() const { return *m_Window; }
        RenderSystem& getRenderSystem() { return m_RenderSystem; }
        SceneManager& getSceneManager() { return m_SceneManager; }
        RuntimeResourceSystem& getResourceSystem() const { return *m_RuntimeResourceSystem; }
        InputLayer& getInputLayer() const { return *m_InputLayer; }
        FrameContext& getFrameContext() { return m_FrameContext; }
        RenderFlags& getRenderFlags() { return m_RenderFlags; }
        EditorRenderExt& getEditorRenderExt() { return m_EditorRenderExt; }
        bool consumePickResult(uint32_t& out_entity_id); // Pop one pending picking result.

        bool isEditMode() const { return m_SceneRunState == SceneRunState::Edit; }
        bool isPlayMode() const { return m_SceneRunState == SceneRunState::Play; }

        std::shared_ptr<Scene> getEditorScene() const { return m_EditorScene; }
        std::shared_ptr<Scene> getRuntimeScene() const { return m_RuntimeScene; }

        std::shared_ptr<Scene> getActiveGameScene() const
        {
            if (m_SceneRunState == SceneRunState::Play && m_RuntimeScene)
                return m_RuntimeScene;
            return m_EditorScene;
        }
        void enterPlayMode();
        void exitPlayMode();

    private:
        float calculateDeltaTime();
        void updateEditMode(float dt);
        void updatePlayMode(float dt);
        std::shared_ptr<Scene> cloneScene(const std::shared_ptr<Scene>& source);

    private:
        bool m_Running = true;    // Main loop running state.
        bool m_Minimized = false; // Window minimized gate.

        SceneRunState m_SceneRunState = SceneRunState::Edit;

        std::shared_ptr<Scene> m_EditorScene;
        std::shared_ptr<Scene> m_RuntimeScene;

        std::shared_ptr<WindowSystem> m_Window;        // Native window wrapper.
        std::unique_ptr<GraphicsContext> m_GraphicsContext; // Graphics backend context.
        LayerStack m_LayerStack;                       // Layer and overlay stack.

        InputLayer* m_InputLayer = nullptr;                    // Input aggregation layer.
        std::shared_ptr<RuntimeResourceSystem> m_RuntimeResourceSystem; // Runtime asset stack.
        RenderSystem m_RenderSystem; // Rendering front-end.
        PhysicsSystem m_PhysicsSystem;
        SceneManager m_SceneManager;                           // Active scene manager.
        FrameContext m_FrameContext{};                         // Per-frame runtime render payload.
        RenderFlags m_RenderFlags = RenderFlags::Forward;      // Enabled render passes this frame.
        EditorRenderExt m_EditorRenderExt{};                   // Optional editor-side render extension.
        bool m_HasPendingPickResult = false;                   // Whether a pick readback is ready.
        uint32_t m_LastPickResult = 0;                         // Last entity id read from ID buffer.

        float m_LastTime = 0.0f; // Previous frame timestamp.
    };
} // namespace Hybrid
