#pragma once
#include "core/event/event.h"
#include "core/event/layer.h"
#include "function/input/input_layer.h"
#include "function/window/window_system.h"
#include "function/render/render_system.h"


namespace Hybrid {

    class HybridEngine {
    public:
        void Init();
        void Run();
        void Shutdown();

        void OnEvent(Event& e);

    private:
        float CalculateDeltaTime();

    private:
        bool m_Running = false;
        bool m_Minimized = false;

        std::shared_ptr<WindowSystem> m_Window;
        LayerStack m_layerStack;
        InputLayer* m_InputLayer = nullptr;
        RenderSystem m_RenderSystem;
    };
}
