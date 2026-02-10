#include "engine.h"

namespace Hybrid {

    void HybridEngine::initialize() {
        LogSystem::initialize();
        m_Window = std::make_shared<WindowSystem>();
        m_Window->initialize(1280, 720, "Hybrid Engine");

        auto surface_io = m_Window->getSurfaceIO();
        surface_io->registerOnEventFunc([this](Event& e) { onEvent(e); });

        //Input Layer
        m_InputLayer = new InputLayer();
        m_layerStack.PushLayer(m_InputLayer);

        //EditorUI
        m_EditorUI = new EditorUI();
        m_EditorUI->initialize(m_Window->getGLFWWindow());

        // RenderSystem 需要在 EditorUI 之后初始化，因为它可能依赖于 ImGui 的上下文
        m_RenderSystem.initialize(m_Window->getGLFWWindow());

    }

    void HybridEngine::run() {
        while (m_Running && !m_Window->shouldClose())
        {

        }

    }

}
