#include "engine.h"


namespace Hybrid {

    void HybridEngine::initialize() {
        LogSystem::initialize();
        m_Window = std::make_shared<WindowSystem>();
        m_Window->initialize(1280, 720, "Hybrid Engine");

        GLFWwindow* window = m_Window->getGLFWWindow();
        if (!window) {
            HBD_CORE_ERROR("GLFW window is null.");
            m_Window->cleanup();
            Hybrid::LogSystem::shutdown();
            return;
        }

        glfwMakeContextCurrent(window);

        const int ver = gladLoadGL(glfwGetProcAddress);
        if (ver == 0) {
            HBD_CORE_ERROR("gladLoadGL failed (returned 0).");
            m_Window->cleanup();
            Hybrid::LogSystem::shutdown();
            return;
        }

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
                m_RenderSystem.renderFrame(
                    m_EditorUI->getViewportSize(),
                    m_Window->getGLFWWindow(),
                    dt,
                    m_EditorUI->isViewportHovered() && m_EditorUI->isViewportFocused(),
                    m_InputLayer->getState()
                );

                m_EditorUI->endFrame();
            }

            // phase 5: events & swap
            m_Window->pollEvents();
            glfwSwapBuffers(m_Window->getGLFWWindow());
        }
    }
    void HybridEngine::onEvent(Event& e)
    {
        // phase 1: input capture (ignore Handled)
        if (m_InputLayer)
            m_InputLayer->onEvent(e);

        // phase 2: system events
        EventDispatcher dispatcher(e);
        dispatcher.dispatch<WindowCloseEvent>([this](WindowCloseEvent&) {
            m_Running = false;
            return true;
            });

        dispatcher.dispatch<WindowResizeEvent>([this](WindowResizeEvent& ev) {
            if (ev.getWidth() == 0 || ev.getHeight() == 0)
            {
                m_Minimized = true;
                return false;
            }
            m_Minimized = false;
            // resize renderer's framebuffer if needed
            
            return false;
            });

        // phase 3: layer dispatch (Handled can break)
        for (auto it = m_LayerStack.rbegin(); it != m_LayerStack.rend(); ++it)
        {
            (*it)->onEvent(e);
            if (e.Handled)
                break;
        }
    }

    void HybridEngine::shutdown()
    {
        if (m_EditorUI)
        {
            m_EditorUI->shutdown();
            delete m_EditorUI;
            m_EditorUI = nullptr;
        }

        delete m_InputLayer;
        m_InputLayer = nullptr;

        m_Window->cleanup();
        LogSystem::shutdown();
    }

    float HybridEngine::calculateDeltaTime()
    {
        float time = static_cast<float>(glfwGetTime());
        float dt = time - m_LastTime;
        m_LastTime = time;
        return dt;
    }
}
