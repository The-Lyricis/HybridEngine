#include "engine.h"
#include <filesystem>
#include "editor/editor_context.h"  
#include <iostream>

namespace Hybrid
{

    void HybridEngine::initialize()
    {
        LogSystem::initialize();
        m_Window = std::make_shared<WindowSystem>();
        m_Window->initialize(1280, 720, "Hybrid Engine");

        GLFWwindow *window = m_Window->getGLFWWindow();
        if (!window)
        {
            HBD_CORE_ERROR("GLFW window is null.");
            m_Window->cleanup();
            Hybrid::LogSystem::shutdown();
            return;
        }
        // GraphicsContext
        m_GraphicsContext = GraphicsContext::Create(window);
        if (!m_GraphicsContext)
        {
            HBD_CORE_ERROR("GraphicsContext creation failed.");
            m_Window->cleanup();
            Hybrid::LogSystem::shutdown();
            return;
        }
        m_GraphicsContext->init();

        // Resource System
        m_ResourceSystem = std::make_shared<ResourceSystem>();
        m_ResourceSystem->initialize();
        m_RenderSystem.setAssetManager(m_ResourceSystem->getManager());

        //--------------- 资源系统测试内容 ---------------
        // auto mgr = m_ResourceSystem->getManager();
        // auto reg = m_ResourceSystem->getRegistry();

        // Hybrid::AssetMetadata meta{};
        // meta.id          = reg->generateUniqueID();
        // meta.type        = Hybrid::AssetType::Texture2D;
        // meta.source_path = "asset:/Textures/rusty_metal_diff_4k.jpg"; // 逻辑路径
        // meta.is_valid    = true;
        // reg->registerAsset(meta);

        // auto tex = mgr->loadSync<Hybrid::Texture>(meta.id);

        auto surface_io = m_Window->getSurfaceIO();
        surface_io->registerOnEventFunc([this](Event &e)
                                        { onEvent(e); });

        // Input Layer
        m_InputLayer = new InputLayer();
        m_LayerStack.pushLayer(m_InputLayer);

        // EditorUI
        m_EditorUI = new EditorUI();
        m_EditorUI->initialize(m_Window->getGLFWWindow());

        // RenderSystem

        m_RenderSystem.initialize(m_Window->getGLFWWindow());

        // SceneSystem
        auto scene = std::make_shared<Scene>();
        m_SceneManager.SetActiveScene(scene);

        //场景绑定到编辑器上下文
        m_EditorUI->setActiveScene(scene.get());


        //--------------- 场景测试内容 ---------------
        // 1) 游戏相机（俯视）
        {
            auto cam = scene->createEntity("Game Camera");
            cam.AddComponent<Hybrid::CameraComponent>(Hybrid::CameraComponent{true, 45.0f, 0.1f, 500.0f});

            auto &tr = cam.GetComponent<Hybrid::TransformComponent>();
            tr.Position = {0.0f, 12.0f, 12.0f};
            tr.Rotation = {glm::radians(-45.0f), 0.0f, 0.0f}; // pitch=-45°, yaw=0, roll=0
            tr.Scale = {1.0f, 1.0f, 1.0f};
        }
        // 2) 添加平行光
        auto sun = scene->createEntity("Sun");
        auto &dl = sun.AddComponent<Hybrid::DirectionalLightComponent>();
        dl.Color = {1.0f, 1.0f, 1.0f};
        dl.Intensity = 1.0f;
        auto &sunTr = sun.GetComponent<Hybrid::TransformComponent>();
        sunTr.Rotation = {glm::radians(-70.5f), glm::radians(-123.7f), 0.0f};

        // 3) 生成一组立方体
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
                    auto cube = scene->createEntity(name);

                    // MeshRenderer：使用内建立方体
                    auto &mr = cube.AddComponent<Hybrid::MeshRendererComponent>();
                    mr.Primitive = 0;

                    // Transform：排布到网格
                    auto &tr = cube.GetComponent<Hybrid::TransformComponent>();
                    tr.Position = {startX + x * spacing, 0.0f, startZ + z * spacing};
                    tr.Rotation = {0.0f, 0.0f, 0.0f};
                    tr.Scale = {1.0f, 1.0f, 1.0f};
                }
            }
        }
        m_RenderSystem.setScene(scene);

        // Prime delta timer to avoid an inflated first-frame dt.
        m_LastTime = static_cast<float>(glfwGetTime());
    }

    void HybridEngine::run()
    {
        while (m_Running && !m_Window->shouldClose())
        {
            float dt = calculateDeltaTime();

            // 即使最小化，也要处理事件，避免窗口“假死”
            if (m_Minimized)
            {
                m_Window->pollEvents();
                m_GraphicsContext->swapBuffers();
                continue;
            }

            // phase 1/2: input & logic update
            for (Layer* layer : m_LayerStack)
                layer->onUpdate(dt);

            if (auto scene = m_SceneManager.GetActiveScene())
                scene->onUpdate(dt);

            // phase 3: events (glfwPollEvents 会驱动 ImGui GLFW backend 的回调)
            m_Window->pollEvents();

            // phase 4: UI begin + panels
            m_EditorUI->beginFrame();
            m_EditorUI->drawPanels();

            // 注意：drawViewport 这一帧会更新 EditorContext 里的 viewport_size/hovered/focused
            // 纹理 ID 通常是“上一帧渲染结果”，这一点与原本逻辑一致（天然一帧延迟显示）。

            auto& ctx = m_EditorUI->context();
            ctx.active_scene = m_SceneManager.GetActiveScene().get();

            // 把上一帧相机矩阵喂给 gizmo（足够稳定）
            ctx.gizmo_view = m_RenderSystem.getLastView();
            ctx.gizmo_proj = m_RenderSystem.getLastProj();

            m_EditorUI->drawViewport(m_RenderSystem.getSceneColorTexture());

            // 将 ImVec2 转为你 RenderSystem 需要的类型（此处示例用 glm::vec2）
            glm::vec2 viewportSize{ ctx.viewport_size.x, ctx.viewport_size.y };


            ImGuiIO& io = ImGui::GetIO();
            bool viewportActive = ctx.viewport_image_hovered;

            const bool useGameCamera = ctx.use_game_camera;

            uint32_t selectedID = (ctx.selected == entt::null) ? 0u : (uint32_t)entt::to_integral(ctx.selected);


            // phase 5: render
            m_RenderSystem.renderFrame(
                viewportSize,
                m_Window->getGLFWWindow(),
                dt,
                viewportActive,
                m_InputLayer->getState(),
                useGameCamera,
                selectedID
            );

            // --- picking：渲染完成后读回 ---
            if (ctx.request_pick)
            {
                uint32_t id = m_RenderSystem.readEntityID(ctx.pick_x, ctx.pick_y);
                ctx.selected = (id == 0) ? entt::null : (entt::entity)id;
                ctx.request_pick = false;
            }

            // phase 6: UI end (提交 ImGui draw data 到默认帧缓冲，确保 UI 覆盖在画面上)
            m_EditorUI->endFrame();

            // phase 7: swap
            m_GraphicsContext->swapBuffers();
        }
    }

    void HybridEngine::onEvent(Event &e)
    {
        // phase 1: input capture (ignore Handled)
        if (m_InputLayer)
            m_InputLayer->onEvent(e);

        // phase 2: system events
        EventDispatcher dispatcher(e);
        dispatcher.dispatch<WindowCloseEvent>([this](WindowCloseEvent &)
                                              {
            m_Running = false;
            return true; });

        dispatcher.dispatch<WindowResizeEvent>([this](WindowResizeEvent &ev)
                                               {
            if (ev.getWidth() == 0 || ev.getHeight() == 0)
            {
                m_Minimized = true;
                return false;
            }
            m_Minimized = false;
            // resize renderer's framebuffer if needed
            m_RenderSystem.onWindowResize(static_cast<uint32_t>(ev.getWidth()),
                                          static_cast<uint32_t>(ev.getHeight()));
            return false; });

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
