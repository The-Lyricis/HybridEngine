#include "engine.h"

#include <algorithm>
#include <string>

#include "runtime/core/base/macro.h"
#include "runtime/core/log/log_system.h"

#include <filesystem>
#include <fstream>
#include "runtime/function/project/project_loader.h"
#include "runtime/function/project/project_context.h"

namespace Hybrid
{
    void HybridEngine::initialize()
    {
        LogSystem::initialize();

        // ===== Project Bootstrap (CWD/GameProject) =====
        namespace fs = std::filesystem;

        const fs::path outputDir = fs::current_path();          // 当前工作目录（输出目录）
        const fs::path projectRoot = outputDir / "GameProject";   // 项目根目录
        const fs::path hyprojPath = projectRoot / "GameProject.hyproj";

        const fs::path assetsDir = projectRoot / "Assets";
        const fs::path cacheDir = projectRoot / "Cache";
        const fs::path buildDir = projectRoot / "Build";
        const fs::path settingsDir = projectRoot / "ProjectSettings";

        std::error_code ec;

        // 1) 创建必要目录
        fs::create_directories(projectRoot, ec);
        if (ec) {
            HBD_CORE_ERROR("Failed to create ProjectRoot: {} ({})", projectRoot.string(), ec.message());
            LogSystem::shutdown();
            return;
        }

        ec.clear();
        fs::create_directories(assetsDir, ec);
        if (ec) {
            HBD_CORE_ERROR("Failed to create Assets dir: {} ({})", assetsDir.string(), ec.message());
            LogSystem::shutdown();
            return;
        }

        ec.clear();
        fs::create_directories(cacheDir, ec);
        if (ec) {
            HBD_CORE_ERROR("Failed to create Cache dir: {} ({})", cacheDir.string(), ec.message());
            LogSystem::shutdown();
            return;
        }

        ec.clear();
        fs::create_directories(buildDir, ec);
        if (ec) {
            HBD_CORE_ERROR("Failed to create Build dir: {} ({})", buildDir.string(), ec.message());
            LogSystem::shutdown();
            return;
        }

        ec.clear();
        fs::create_directories(settingsDir, ec);
        if (ec) {
            HBD_CORE_ERROR("Failed to create ProjectSettings dir: {} ({})", settingsDir.string(), ec.message());
            LogSystem::shutdown();
            return;
        }

        // 2) 生成默认 hyproj（如果不存在）
        if (!fs::exists(hyprojPath))
        {
            std::ofstream ofs(hyprojPath, std::ios::out | std::ios::binary);
            if (!ofs) {
                HBD_CORE_ERROR("Failed to create hyproj: {}", hyprojPath.string());
                LogSystem::shutdown();
                return;
            }

            ofs << "# Auto-generated project file (debug)\n";
            ofs << "name=GameProject\n";
            ofs << "assets=Assets\n";
            ofs << "cache=Cache\n";
            ofs << "build=Build\n";
            ofs << "settings=ProjectSettings\n";
            ofs.close();

            HBD_CORE_INFO("Created default hyproj: {}", fs::absolute(hyprojPath).string());
        }

        // 3) Load hyproj -> ProjectContext，并写入 ProjectService
        Hybrid::ProjectContext pctx;
        std::string perr;
        if (!Hybrid::ProjectLoader::LoadFromFile(hyprojPath, pctx, perr))
        {
            HBD_CORE_ERROR("Project load failed: {}", perr);
            HBD_CORE_ERROR("hyproj: {}", fs::absolute(hyprojPath).string());
            LogSystem::shutdown();
            return;
        }

        Hybrid::ProjectService::Set(pctx);

        HBD_CORE_INFO("Project loaded: {}", fs::absolute(hyprojPath).string());
        HBD_CORE_INFO("Project Root   : {}", pctx.root.string());
        HBD_CORE_INFO("Assets         : {}", pctx.assets.string());
        HBD_CORE_INFO("Cache          : {}", pctx.cache.string());
        HBD_CORE_INFO("Build          : {}", pctx.build.string());
        HBD_CORE_INFO("ProjectSettings: {}", pctx.settings.string());
        // =============================================

        // ===== Window / Graphics =====
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

        // ===== Resource System (Project-based) =====
        m_RuntimeResourceSystem = std::make_shared<RuntimeResourceSystem>();
        // 这里传 nullptr：RuntimeResourceSystem 内部会 fallback 创建 NativeFileSystem 并按 ctx 挂载（你已在 StepA 做过）
        m_RuntimeResourceSystem->initialize(Hybrid::ProjectService::Get(), nullptr);

        m_RenderSystem.setAssetManager(m_RuntimeResourceSystem->getManager());

        // ===== Event / Layers =====
        auto surface_io = m_Window->getSurfaceIO();
        surface_io->registerOnEventFunc([this](Event& e) { onEvent(e); });

        m_InputLayer = new InputLayer();
        pushLayer(m_InputLayer);

        // ===== Render =====
        m_RenderSystem.initialize(window);

        // ===== Scene =====
        auto scene = std::make_shared<Scene>();
        m_SceneManager.setActiveScene(scene);
        m_RenderSystem.setScene(scene);
        m_FrameContext.scene = scene;
        m_FrameContext.window_handle = window;

        {
            auto cam = scene->createEntity("Game Camera");
            cam.AddComponent<Hybrid::CameraComponent>(Hybrid::CameraComponent{ true, 45.0f, 0.1f, 500.0f });

            auto& tr = cam.GetComponent<Hybrid::TransformComponent>();
            tr.Position = { 0.0f, 12.0f, 12.0f };
            tr.Rotation = { glm::radians(-45.0f), 0.0f, 0.0f };
            tr.Scale = { 1.0f, 1.0f, 1.0f };
        }

        auto sun = scene->createEntity("Sun");
        auto& dl = sun.AddComponent<Hybrid::DirectionalLightComponent>();
        dl.Color = { 1.0f, 1.0f, 1.0f };
        dl.Intensity = 1.0f;
        auto& sunTr = sun.GetComponent<Hybrid::TransformComponent>();
        sunTr.Rotation = { glm::radians(-70.5f), glm::radians(-123.7f), 0.0f };

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
                    tr.Position = { startX + x * spacing, 0.0f, startZ + z * spacing };
                    tr.Rotation = { 0.0f, 0.0f, 0.0f };
                    tr.Scale = { 1.0f, 1.0f, 1.0f };
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
