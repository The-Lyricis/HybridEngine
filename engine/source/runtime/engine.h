#pragma once
#include <memory>

#include "runtime/function/window/window_system.h"
#include "runtime/core/event/layer.h"
#include "runtime/core/event/application_event.h"
#include "runtime/core/event/input_event.h"
#include "runtime/function/input/input_layer.h"
#include "runtime/function/render/render_system.h"
#include "runtime/function/render/graphics_context.h"
#include "editor/include/editor_ui.h"
#include "function/scene/scene_manager.h"
#include "function/scene/scene.h"


namespace Hybrid{

    class HybridEngine {
    public:
        void initialize();
        void run();
        void shutdown();

        void onEvent(Event& e);

    private:
        float calculateDeltaTime();

    private:
        bool m_Running = true;
        bool m_Minimized = false;   //用来判断窗口是否被最小化

        std::shared_ptr<WindowSystem> m_Window;
        std::unique_ptr<GraphicsContext> m_GraphicsContext;
        LayerStack m_LayerStack;

        InputLayer* m_InputLayer = nullptr;
        EditorUI* m_EditorUI = nullptr;
        RenderSystem m_RenderSystem;
        SceneManager m_SceneManager;

        float m_LastTime = 0.0f;    //保存上一帧时间
    };
}
