#include "engine.h"
#include <filesystem>


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
        // GraphicsContext
        m_GraphicsContext = GraphicsContext::Create(window);
        if (!m_GraphicsContext) {
            HBD_CORE_ERROR("GraphicsContext creation failed.");
            m_Window->cleanup();
            Hybrid::LogSystem::shutdown();
            return;
        }
        m_GraphicsContext->init();

        // Resource System
        m_ResourceSystem = std::make_shared<ResourceSystem>();
        m_ResourceSystem->initialize();

        auto surface_io = m_Window->getSurfaceIO();
        surface_io->registerOnEventFunc([this](Event& e) { onEvent(e); });

        //Input Layer
        m_InputLayer = new InputLayer();
        m_LayerStack.pushLayer(m_InputLayer);

        //EditorUI
        m_EditorUI = new EditorUI();
        m_EditorUI->initialize(m_Window->getGLFWWindow());

        // RenderSystem 
        m_RenderSystem.initialize(m_Window->getGLFWWindow());

        // SceneSystem
        auto scene = std::make_shared<Scene>();
        m_SceneManager.SetActiveScene(scene);

        //--------------- 场景测试内容 ---------------
        // 1) 游戏相机（俯视）
        {
            auto cam = scene->CreateEntity("Game Camera");
            cam.AddComponent<Hybrid::CameraComponent>(Hybrid::CameraComponent{ true, 45.0f, 0.1f, 500.0f });

            auto& tr = cam.GetComponent<Hybrid::TransformComponent>();
            tr.Position = { 0.0f, 12.0f, 12.0f };
            tr.Rotation = { glm::radians(-45.0f), 0.0f, 0.0f }; // pitch=-45°, yaw=0, roll=0
            tr.Scale = { 1.0f, 1.0f, 1.0f };
        }

        // 2) 生成一组立方体
        {
            const int gridX = 5;
            const int gridZ = 5;
            const float spacing = 2.0f;

            // 让网格以原点为中心
            const float startX = -0.5f * (gridX - 1) * spacing;
            const float startZ = -0.5f * (gridZ - 1) * spacing;

            for (int z = 0; z < gridZ; ++z)
            {
                for (int x = 0; x < gridX; ++x)
                {
                    std::string name = "Cube_" + std::to_string(z) + "_" + std::to_string(x);
                    auto cube = scene->CreateEntity(name);

                    // MeshRenderer：使用内建立方体
                    auto& mr = cube.AddComponent<Hybrid::MeshRendererComponent>();
                    mr.Primitive = 0;

                    // Transform：排布到网格
                    auto& tr = cube.GetComponent<Hybrid::TransformComponent>();
                    tr.Position = { startX + x * spacing, 0.0f, startZ + z * spacing };
                    tr.Rotation = { 0.0f, 0.0f, 0.0f };
                    tr.Scale = { 1.0f, 1.0f, 1.0f };
                }
            }
        }
        m_RenderSystem.setScene(scene);

    }

    void HybridEngine::run() {
        while (m_Running && !m_Window->shouldClose())
        {
            float dt = calculateDeltaTime();

            // phase 1: input state update
            if (!m_Minimized)
            {
                // phase 2: logic update
                for (Layer* layer : m_LayerStack)
                    layer->onUpdate(dt);

                // 场景更新
                if (auto scene = m_SceneManager.GetActiveScene())
                    scene->OnUpdate(dt);


                // 注意：事件轮询应该在所有层更新之后进行，以确保事件能被当帧更新的层捕获
                m_Window->pollEvents();
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
                    m_InputLayer->getState(),
                    m_EditorUI->useGameCamera()   // <-- 新增：UI 控制的模式
                );
                m_EditorUI->endFrame();
            }

            // phase 5: events & swap
            m_GraphicsContext->swapBuffers();
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
