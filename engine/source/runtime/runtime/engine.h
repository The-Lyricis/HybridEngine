#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <chrono>
#include <functional>

#include "runtime/core/event/application_event.h"
#include "runtime/core/event/input_event.h"
#include "runtime/core/event/layer.h"
#include "runtime/core/job/job_system.h"
#include "runtime/core/time/frame_clock.h"
#include "runtime/modules/asset/runtime_resource_system.h"
#include "runtime/modules/input/input_layer.h"
#include "runtime/modules/render/runtime/frame_context.h"
#include "runtime/modules/render/runtime/render_frame_request.h"
#include "runtime/modules/render/public/graphics_context.h"
#include "runtime/modules/render/runtime/render_flags.h"
#include "runtime/modules/render/runtime/render_system.h"
#include "runtime/modules/scene/scene.h"
#include "runtime/modules/scene/scene_manager.h"
#include "runtime/modules/window/window_system.h"
#include "runtime/modules/physics/physics_system.h"
#include "runtime/modules/scene/components/rigidbody_component.h"

namespace Hybrid
{
    struct EngineConfig
    {
        std::filesystem::path project_path;
        bool headless = false;
        bool window_visible = true;
        float fixed_update_hz = 60.0f;
        std::size_t worker_count = 0;
    };

    // Runtime application host. Editor extends behavior via layers/services.
    class HybridEngine
    {
    public:
        ~HybridEngine() { shutdown(); }
        bool initialize(const EngineConfig& config);
        bool initialize(const std::filesystem::path& project_path = {});
        void run(uint64_t max_frames = 0);
        void shutdown() noexcept;
        void requestExit() noexcept;
        void setExitRequestHandler(std::function<void()> handler) { m_ExitRequestHandler = std::move(handler); }

        void onEvent(Event& e);          // Dispatch platform input/window events.
        Layer& pushLayer(std::unique_ptr<Layer> layer);    // Insert gameplay/editor layer.
        Layer& pushOverlay(std::unique_ptr<Layer> layer);  // Insert overlay layer (e.g. ImGui).

        WindowSystem& getWindowSystem() const { return *m_Window; }
        RenderSystem& getRenderSystem() { return m_RenderSystem; }
        SceneManager& getSceneManager() { return m_SceneManager; }
        RuntimeResourceSystem& getResourceSystem() const { return *m_RuntimeResourceSystem; }
        InputLayer& getInputLayer() const { return *m_InputLayer; }
        FrameContext& getFrameContext() { return m_FrameContext; }
        RenderFlags& getRenderFlags() { return m_RenderFlags; }
        RenderFrameRequest& getRenderFrameRequest() { return m_RenderFrameRequest; }
        const RenderFrameResult& getRenderFrameResult() const { return m_RenderFrameResult; }
        bool consumePickResult(uint32_t& out_entity_id); // Pop one pending picking result.

        std::shared_ptr<Scene> getActiveScene() const { return m_ActiveScene; }
        bool setActiveScene(std::shared_ptr<Scene> scene);
        void setFixedUpdateEnabled(bool enabled) { m_FixedUpdateEnabled = enabled; }
        void setSceneUpdateEnabled(bool enabled) { m_SceneUpdateEnabled = enabled; }
        bool isHeadless() const { return m_Headless; }
        bool isInitialized() const { return m_Initialized; }
        std::shared_ptr<JobSystem> getJobSystem() const { return m_JobSystem; }

    private:
        float calculateDeltaTime();
        void updateActiveScene(float dt);

    private:
        bool m_Running = true;    // Main loop running state.
        bool m_Minimized = false; // Window minimized gate.

        std::shared_ptr<Scene> m_ActiveScene;

        std::shared_ptr<WindowSystem> m_Window;        // Native window wrapper.
        std::unique_ptr<GraphicsContext> m_GraphicsContext; // Graphics backend context.
        LayerStack m_LayerStack;                       // Layer and overlay stack.

        std::unique_ptr<InputLayer> m_InputLayer;               // Input aggregation service.
        std::shared_ptr<RuntimeResourceSystem> m_RuntimeResourceSystem; // Runtime asset stack.
        std::shared_ptr<JobSystem> m_JobSystem;
        RenderSystem m_RenderSystem; // Rendering front-end.
        PhysicsSystem m_PhysicsSystem;
        SceneManager m_SceneManager;                           // Active scene manager.
        FrameContext m_FrameContext{};                         // Per-frame runtime render payload.
        RenderFlags m_RenderFlags = RenderFlags::Scene;        // Enabled render passes this frame.
        RenderFrameRequest m_RenderFrameRequest{};
        RenderFrameResult m_RenderFrameResult{};
        bool m_HasPendingPickResult = false;                   // Whether a pick readback is ready.
        uint32_t m_LastPickResult = kInvalidEntityID;          // Last entity id read from ID buffer.

        std::chrono::steady_clock::time_point m_LastFrameTime{};
        FrameClock m_FrameClock;
        bool m_Headless = false;
        bool m_Initialized = false;
        bool m_FixedUpdateEnabled = false;
        bool m_SceneUpdateEnabled = true;
        std::function<void()> m_ExitRequestHandler;
    };
} // namespace Hybrid
