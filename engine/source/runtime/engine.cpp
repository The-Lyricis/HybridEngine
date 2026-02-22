#include "engine.h"

#include <algorithm>
#include <string>

#include "runtime/core/base/macro.h"
#include "runtime/core/log/log_system.h"

namespace Hybrid
{
    void HybridEngine::initialize()
    {
        LogSystem::initialize();
        m_Window = std::make_shared<WindowSystem>();
        m_Window->initialize(1280, 720, "Hybrid Engine");

        GLFWwindow* window = m_Window->getNativeWindow();
        if (!window)
        {
            HBD_CORE_ERROR("GLFW window is null.");
            m_Window->cleanup();
            LogSystem::shutdown();
            return;
        }

        m_GraphicsContext = GraphicsContext::Create(window);
        if (!m_GraphicsContext)
        {
            HBD_CORE_ERROR("GraphicsContext creation failed.");
            m_Window->cleanup();
            LogSystem::shutdown();
            return;
        }
        m_GraphicsContext->init();

        m_ResourceSystem = std::make_shared<RuntimeResourceSystem>();
        m_ResourceSystem->initialize();
        m_RenderSystem.setAssetManager(m_ResourceSystem->getManager());

        auto surface_io = m_Window->getSurfaceIO();
        surface_io->registerOnEventFunc([this](Event& e) { onEvent(e); });

        m_InputLayer = new InputLayer();
        pushLayer(m_InputLayer);

        m_RenderSystem.initialize(window);

        auto scene = std::make_shared<Scene>();
        m_SceneManager.setActiveScene(scene);
        m_RenderSystem.setScene(scene);
        m_FrameContext.scene = scene;
        m_FrameContext.window_handle = window;

        {
            auto cam = scene->createEntity("Game Camera");
            cam.AddComponent<Hybrid::CameraComponent>(Hybrid::CameraComponent{true, 45.0f, 0.1f, 500.0f});

            auto& tr = cam.GetComponent<Hybrid::TransformComponent>();
            tr.Position = {0.0f, 12.0f, 12.0f};
            tr.Rotation = {glm::radians(-45.0f), 0.0f, 0.0f};
            tr.Scale = {1.0f, 1.0f, 1.0f};
        }

        auto sun = scene->createEntity("Sun");
        auto& dl = sun.AddComponent<Hybrid::DirectionalLightComponent>();
        dl.Color = {1.0f, 1.0f, 1.0f};
        dl.Intensity = 1.0f;
        auto& sunTr = sun.GetComponent<Hybrid::TransformComponent>();
        sunTr.Rotation = {glm::radians(-70.5f), glm::radians(-123.7f), 0.0f};

        {
            const int gridX = 5;
            const int gridZ = 5;
            const float spacing = 2.0f;
            const float startX = -0.5f * (gridX - 1) * spacing;
            const float startZ = -0.5f * (gridZ - 1) * spacing;

            for (int z = 0; z < gridZ; ++z)
            {
                for (int x = 0; x < gridX; ++x)
                {
                    std::string name = "Cube_" + std::to_string(z) + "_" + std::to_string(x);
                    auto cube = scene->createEntity(name);

                    auto& mr = cube.AddComponent<Hybrid::MeshRendererComponent>();
                    mr.Primitive = 0;

                    auto& tr = cube.GetComponent<Hybrid::TransformComponent>();
                    tr.Position = {startX + x * spacing, 0.0f, startZ + z * spacing};
                    tr.Rotation = {0.0f, 0.0f, 0.0f};
                    tr.Scale = {1.0f, 1.0f, 1.0f};
                }
            }
        }

        int fbw = 0, fbh = 0;
        glfwGetFramebufferSize(window, &fbw, &fbh);
        m_FrameContext.viewport_size.x = static_cast<float>(std::max(1, fbw));
        m_FrameContext.viewport_size.y = static_cast<float>(std::max(1, fbh));

        m_LastTime = static_cast<float>(glfwGetTime());
    }

    void HybridEngine::run()
    {
        while (m_Running && !m_Window->shouldClose())
        {
            const float dt = calculateDeltaTime();

            if (m_Minimized)
            {
                m_Window->pollEvents();
                m_GraphicsContext->swapBuffers();
                continue;
            }

            for (Layer* layer : m_LayerStack)
            {
                layer->onUpdate(dt);
            }

            if (auto scene = m_SceneManager.getActiveScene())
            {
                scene->onUpdate(dt);
            }

            m_Window->pollEvents();

            m_FrameContext.dt = dt;
            m_FrameContext.input = &m_InputLayer->getState();
            m_FrameContext.scene = m_SceneManager.getActiveScene();

            glm::vec2 viewport_size = m_FrameContext.viewport_size;
            if (viewport_size.x <= 0.0f || viewport_size.y <= 0.0f)
            {
                int fbw = 0, fbh = 0;
                glfwGetFramebufferSize(m_Window->getNativeWindow(), &fbw, &fbh);
                viewport_size.x = static_cast<float>(std::max(1, fbw));
                viewport_size.y = static_cast<float>(std::max(1, fbh));
                m_FrameContext.viewport_size = viewport_size;
            }

            m_RenderSystem.renderFrame(m_FrameContext, m_RenderFlags, &m_EditorRenderExt);

            for (Layer* layer : m_LayerStack)
            {
                layer->onImGuiRender();
            }

            if (m_EditorRenderExt.request_pick && HasFlag(m_RenderFlags, RenderFlags::PickingID))
            {
                m_LastPickResult = m_RenderSystem.readEntityID(m_EditorRenderExt.pick_x, m_EditorRenderExt.pick_y);
                m_HasPendingPickResult = true;
                m_EditorRenderExt.request_pick = false;
            }

            m_GraphicsContext->swapBuffers();
        }
    }

    void HybridEngine::onEvent(Event& e)
    {
        if (m_InputLayer)
        {
            m_InputLayer->onEvent(e);
        }

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
            m_RenderSystem.onWindowResize(static_cast<uint32_t>(ev.getWidth()), static_cast<uint32_t>(ev.getHeight()));
            m_FrameContext.viewport_size = {static_cast<float>(ev.getWidth()), static_cast<float>(ev.getHeight())};
            return false;
        });

        for (auto it = m_LayerStack.rbegin(); it != m_LayerStack.rend(); ++it)
        {
            (*it)->onEvent(e);
            if (e.Handled)
            {
                break;
            }
        }
    }

    void HybridEngine::pushLayer(Layer* layer)
    {
        if (!layer)
            return;
        m_LayerStack.pushLayer(layer);
        layer->onAttach();
    }

    void HybridEngine::pushOverlay(Layer* layer)
    {
        if (!layer)
            return;
        m_LayerStack.pushOverlay(layer);
        layer->onAttach();
    }

    bool HybridEngine::consumePickResult(uint32_t& out_entity_id)
    {
        if (!m_HasPendingPickResult)
            return false;

        out_entity_id = m_LastPickResult;
        m_HasPendingPickResult = false;
        return true;
    }

    void HybridEngine::shutdown()
    {
        m_LayerStack.clear();
        m_InputLayer = nullptr;

        if (m_Window)
        {
            m_Window->cleanup();
        }

        LogSystem::shutdown();
    }

    float HybridEngine::calculateDeltaTime()
    {
        const float time = static_cast<float>(glfwGetTime());
        const float dt = time - m_LastTime;
        m_LastTime = time;
        return dt;
    }
} // namespace Hybrid
