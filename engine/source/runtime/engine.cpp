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
        m_LayerStack.pushLayer(m_InputLayer);

        //EditorUI
        m_EditorUI = new EditorUI();
        m_EditorUI->initialize(m_Window->getGLFWWindow());

        // RenderSystem 需要在 EditorUI 之后初始化，因为它可能依赖于 ImGui 的上下文
        m_RenderSystem.initialize(m_Window->getGLFWWindow());

    }

    void HybridEngine::run() {
        while (m_Running && !m_Window->shouldClose())
        {
            float dt = calculateDeltaTime();

            // phase 1: input state update
            m_InputLayer->onUpdate(dt);

            if (!m_Minimized)
            {
                // phase 2: logic update
                for (Layer* layer : m_LayerStack)
                    layer->onUpdate(dt);

                // phase 3: UI
                m_EditorUI->beginFrame();
                m_EditorUI->drawPanels();
                m_EditorUI->drawViewport(m_RenderSystem.getSceneColorTexture());

                // phase 4: render


                m_EditorUI->endFrame();
            }

            // phase 5: events & swap
            m_Window->pollEvents();
            glfwSwapBuffers(m_Window->getGLFWWindow());
        }
    }
}
