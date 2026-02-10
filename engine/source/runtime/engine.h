#pragma once
#include "core/event/event.h"
#include "core/event/layer.h"
#include "function/input/input_layer.h"
#include "function/window/window_system.h"
#include "function/render/render_system.h"
#include <editor/include/editor_ui.h>


namespace Hybrid {

    class HybridEngine {
    public:
        void initialize();
        void run();
        void shutdown();

        void onEvent(Event& e);

    private:
        float calculateDeltaTime();

    private:
        bool m_Running = false;
        bool m_Minimized = false;   //用来判断窗口是否被最小化

        std::shared_ptr<WindowSystem> m_Window;
        LayerStack m_layerStack;

        InputLayer* m_InputLayer = nullptr;
        EditorUI* m_EditorUI = nullptr;
        RenderSystem m_RenderSystem;

        float m_LastTime = 0.0f;    //保存上一帧时间
    };
}
