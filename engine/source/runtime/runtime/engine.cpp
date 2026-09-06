#include "engine.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>

#include "runtime/core/base/macro.h"
#include "runtime/core/base/math_util.h"
#include "runtime/core/log/log_system.h"

#include <filesystem>
#include "runtime/modules/project/project_creator.h"
#include "runtime/modules/project/project_paths.h"
#include "runtime/modules/project/project_loader.h"
#include "runtime/modules/project/project_context.h"
#include "runtime/modules/scene/scene_serializer.h"

namespace Hybrid
{
    namespace
    {
        constexpr const char* kEngineLogTag = "[Engine]";

        std::string pathOrPlaceholder(const std::filesystem::path& path)
        {
            return path.empty() ? std::string("<empty>") : path.generic_string();
        }

        const char* sceneNameOrPlaceholder(const std::shared_ptr<Scene>& scene)
        {
            if (!scene)
                return "<null>";
            if (scene->getName().empty())
                return "<unnamed>";
            return scene->getName().c_str();
        }

    } // namespace

    bool HybridEngine::initialize(const std::filesystem::path& project_path)
    {
        EngineConfig config{};
        config.project_path = project_path;
        return initialize(config);
    }

    bool HybridEngine::initialize(const EngineConfig& config)
    {
        if (m_Initialized)
            return true;
        LogSystem::initialize();
        m_Running = true;
        m_Minimized = false;
        m_Headless = config.headless;
        m_FrameClock.configure({config.fixed_update_hz, 4});
        m_FixedUpdateEnabled = false;
        m_SceneUpdateEnabled = true;

        namespace fs = std::filesystem;
        const fs::path cwd = fs::current_path();
        fs::path hyproj_path;
        if (config.project_path.empty())
        {
            const ProjectCreateDesc bootstrap_desc = MakeDebugBootstrapProjectDesc(cwd);
            HBD_CORE_INFO("{} debug_project_bootstrap_started cwd={} project_root={}",
                          kEngineLogTag,
                          pathOrPlaceholder(cwd),
                          pathOrPlaceholder(bootstrap_desc.project_root));

            std::string create_error;
            if (!ProjectCreator::CreateProject(bootstrap_desc, hyproj_path, create_error))
            {
                HBD_CORE_ERROR("{} project_bootstrap_failed cwd={} reason={}",
                               kEngineLogTag,
                               pathOrPlaceholder(cwd),
                               create_error);
                LogSystem::shutdown();
                return false;
            }
        }
        else
        {
            std::string resolve_error;
            if (!ResolveProjectFilePath(config.project_path, hyproj_path, resolve_error))
            {
                HBD_CORE_ERROR("{} project_path_resolve_failed requested_path={} reason={}",
                               kEngineLogTag,
                               pathOrPlaceholder(config.project_path),
                               resolve_error);
                LogSystem::shutdown();
                return false;
            }

            HBD_CORE_INFO("{} initialize_started cwd={} requested_project={} resolved_hyproj={}",
                          kEngineLogTag,
                          pathOrPlaceholder(cwd),
                          pathOrPlaceholder(config.project_path),
                          pathOrPlaceholder(hyproj_path));
        }

        Hybrid::ProjectContext pctx;
        std::string perr;
        if (!Hybrid::ProjectLoader::LoadFromFile(hyproj_path, pctx, perr))
        {
            HBD_CORE_ERROR("{} project_load_failed hyproj={} reason={}",
                           kEngineLogTag,
                           pathOrPlaceholder(fs::absolute(hyproj_path)),
                           perr);
            LogSystem::shutdown();
            return false;
        }

        Hybrid::ProjectService::Set(pctx);
        HBD_CORE_INFO("{} project_loaded hyproj={} project_root={} assets_root={} cache_root={} build_root={} settings_root={}",
                      kEngineLogTag,
                      pathOrPlaceholder(fs::absolute(hyproj_path)),
                      pathOrPlaceholder(pctx.root),
                      pathOrPlaceholder(pctx.assets),
                      pathOrPlaceholder(pctx.cache),
                      pathOrPlaceholder(pctx.build),
                      pathOrPlaceholder(pctx.settings));

        m_JobSystem = std::make_shared<JobSystem>();
        try
        {
            m_JobSystem->initialize(JobSystemConfig{config.worker_count});
        }
        catch (const std::exception& error)
        {
            HBD_CORE_ERROR("{} initialize_failed step=job_system reason={}", kEngineLogTag, error.what());
            shutdown();
            return false;
        }

        GLFWwindow *window = nullptr;
        if (!m_Headless)
        {
            try
            {
                m_Window = std::make_shared<WindowSystem>();
                m_Window->initialize(1280, 720, "Hybrid Engine", config.window_visible);
                window = m_Window->getNativeWindow();
                m_GraphicsContext = GraphicsContext::Create(window);
                if (!window || !m_GraphicsContext)
                    throw std::runtime_error("window or graphics context creation failed");
                m_GraphicsContext->init();
            }
            catch (const std::exception& error)
            {
                HBD_CORE_ERROR("{} initialize_failed step=window_graphics reason={}", kEngineLogTag, error.what());
                shutdown();
                return false;
            }
        }

        // ===== Resource System (Project-based) =====
        m_RuntimeResourceSystem = std::make_shared<RuntimeResourceSystem>();

        // Pass nullptr VFS to let RuntimeResourceSystem create the default NativeFileSystem.
        if (!m_RuntimeResourceSystem->initialize(Hybrid::ProjectService::Get(), nullptr, m_JobSystem))
        {
            HBD_CORE_ERROR("{} initialize_failed step=resource_system", kEngineLogTag);
            shutdown();
            return false;
        }

        if (!m_Headless)
            m_RenderSystem.setAssetManager(m_RuntimeResourceSystem->getManager());

        // ===== Event / Layers =====
        if (m_Window)
        {
            auto surface_io = m_Window->getSurfaceIO();
            surface_io->registerOnEventFunc([this](Event &e) { onEvent(e); });
        }

        m_InputLayer = std::make_unique<InputLayer>();

        // ===== Render =====
        if (!m_Headless)
        {
            m_RenderSystem.initialize(window);
            if (!m_RenderSystem.isInitialized())
            {
                HBD_CORE_ERROR("{} initialize_failed step=render_system", kEngineLogTag);
                shutdown();
                return false;
            }
        }

        // ===== Scene =====
        auto scene = std::make_shared<Scene>();
        scene->setName("Untitled");
        m_FrameContext.window_handle = window;

        if (!setActiveScene(scene))
        {
            HBD_CORE_ERROR("{} initialize_failed step=active_scene_bind scene={} reason=set_active_scene_failed",
                           kEngineLogTag,
                           sceneNameOrPlaceholder(scene));
            shutdown();
            return false;
        }
        m_FrameContext.window_handle = window;

        int fbw = 1, fbh = 1;
        if (window)
            glfwGetFramebufferSize(window, &fbw, &fbh);
        m_FrameContext.viewport_size.x = static_cast<float>(std::max(1, fbw));
        m_FrameContext.viewport_size.y = static_cast<float>(std::max(1, fbh));
        m_RenderFrameRequest.scene = scene;
        m_RenderFrameRequest.window_handle = window;
        RenderViewRequest default_view{};
        default_view.name = "Game";
        default_view.kind = RenderViewKind::Game;
        default_view.size = m_FrameContext.viewport_size;
        default_view.id = 1;
        m_RenderFrameRequest.views.push_back(std::move(default_view));

        m_LastFrameTime = std::chrono::steady_clock::now();

        //物理系统初始化
        m_PhysicsSystem.initialize();
        m_Initialized = true;
        m_FixedUpdateEnabled = false;
        HBD_CORE_INFO("{} initialize_completed scene={} viewport={}x{}",
                      kEngineLogTag,
                      sceneNameOrPlaceholder(m_ActiveScene),
                      static_cast<int>(m_FrameContext.viewport_size.x),
                      static_cast<int>(m_FrameContext.viewport_size.y));
        return true;
    }

    bool HybridEngine::setActiveScene(std::shared_ptr<Scene> scene)
    {
        if (!scene)
        {
            HBD_CORE_WARN("{} active_scene_set_rejected reason=null_scene", kEngineLogTag);
            return false;
        }

        m_ActiveScene = std::move(scene);
        m_FrameClock.reset();
        m_HasPendingPickResult = false;
        m_LastPickResult = kInvalidEntityID;
        m_SceneManager.setActiveScene(m_ActiveScene);
        if (!m_Headless)
            m_RenderSystem.setScene(m_ActiveScene);
        m_FrameContext.scene = m_ActiveScene;
        m_RenderFrameRequest.scene = m_ActiveScene;
        return m_SceneManager.getActiveScene() == m_ActiveScene;
    }

    void HybridEngine::run(uint64_t max_frames)
    {
        if (!m_Initialized)
            return;
        HBD_CORE_INFO("{} run_started", kEngineLogTag);
        uint64_t frame_count = 0;
        while (m_Running && (m_Headless || (m_Window && !m_Window->shouldClose())))
        {
            const float dt = calculateDeltaTime();
            if (m_Window)
                m_Window->pollEvents();

            for (const auto& layer : m_LayerStack)
            {
                layer->onBeginFrame();
            }

            if (m_Minimized)
            {
                for (auto it = m_LayerStack.rbegin(); it != m_LayerStack.rend(); ++it)
                {
                    (*it)->onEndFrame();
                }
                m_InputLayer->onEndFrame();
                if (m_GraphicsContext)
                    m_GraphicsContext->swapBuffers();
                ++frame_count;
                if (max_frames != 0 && frame_count >= max_frames)
                    m_Running = false;
                continue;
            }

            for (const auto& layer : m_LayerStack)
            {
                layer->onUpdate(dt);
            }

            updateActiveScene(dt);

            m_FrameContext.dt = dt;
            m_FrameContext.input = &m_InputLayer->getState();
            m_FrameContext.scene = m_ActiveScene;
            if (!m_Headless)
                m_RenderSystem.update(dt);

            glm::vec2 viewport_size = m_FrameContext.viewport_size;
            if (viewport_size.x <= 0.0f || viewport_size.y <= 0.0f)
            {
                int fbw = 0, fbh = 0;
                if (m_Window)
                    glfwGetFramebufferSize(m_Window->getNativeWindow(), &fbw, &fbh);
                viewport_size.x = static_cast<float>(std::max(1, fbw));
                viewport_size.y = static_cast<float>(std::max(1, fbh));
                m_FrameContext.viewport_size = viewport_size;
            }

            if (!m_Headless)
            {
                m_RenderFrameRequest.scene = m_FrameContext.scene;
                m_RenderFrameRequest.dt = dt;
                m_RenderFrameRequest.window_handle = m_FrameContext.window_handle;
                m_RenderFrameRequest.input = m_FrameContext.input;
                if (m_RenderFrameRequest.views.empty())
                    m_RenderFrameRequest.views.push_back({"Game", RenderViewKind::Game, viewport_size, m_RenderFlags});
                else if (m_RenderFrameRequest.views.size() == 1 &&
                         m_RenderFrameRequest.views.front().id == 1 &&
                         m_RenderFrameRequest.views.front().kind == RenderViewKind::Game)
                    m_RenderFrameRequest.views.front().size = viewport_size;
                m_RenderFrameResult = m_RenderSystem.renderFrame(m_RenderFrameRequest);
            }

            for (const auto& layer : m_LayerStack)
            {
                layer->onImGuiRender();
            }

            if (!m_Headless)
            {
                for (const RenderViewResult& view_result : m_RenderFrameResult.views)
                {
                    if (view_result.picked_entity)
                    {
                        m_LastPickResult = *view_result.picked_entity;
                        m_HasPendingPickResult = true;
                        break;
                    }
                }
            }

            for (auto it = m_LayerStack.rbegin(); it != m_LayerStack.rend(); ++it)
            {
                (*it)->onEndFrame();
            }
            m_InputLayer->onEndFrame();

            if (m_GraphicsContext)
                m_GraphicsContext->swapBuffers();
            ++frame_count;
            if (max_frames != 0 && frame_count >= max_frames)
                m_Running = false;
        }

        HBD_CORE_INFO("{} run_stopped reason={}",
                      kEngineLogTag,
                      (m_Window && m_Window->shouldClose()) ? "window_should_close" : "running_flag_false");
    }

    void HybridEngine::onEvent(Event &e)
    {
        if (m_InputLayer)
        {
            m_InputLayer->onEvent(e);
        }

        EventDispatcher dispatcher(e);
        dispatcher.dispatch<WindowCloseEvent>([this](WindowCloseEvent &)
                                              {
            HBD_CORE_INFO("{} window_close_requested", kEngineLogTag);
            if (m_ExitRequestHandler)
            {
                if (m_Window)
                    m_Window->setShouldClose(false);
                m_ExitRequestHandler();
            }
            else
            {
                requestExit();
            }
            return true; });

        dispatcher.dispatch<WindowResizeEvent>([this](WindowResizeEvent &ev)
                                               {
             if (ev.getWidth() == 0 || ev.getHeight() == 0)
             {
                 if (!m_Minimized)
                 {
                     HBD_CORE_DEBUG("{} window_minimized", kEngineLogTag);
                 }
                 m_Minimized = true;
                 return false;
             }

             m_Minimized = false;
             m_RenderSystem.onWindowResize(static_cast<uint32_t>(ev.getWidth()), static_cast<uint32_t>(ev.getHeight()));
             m_FrameContext.viewport_size = {static_cast<float>(ev.getWidth()), static_cast<float>(ev.getHeight())};
             HBD_CORE_DEBUG("{} window_resized width={} height={}",
                            kEngineLogTag,
                            ev.getWidth(),
                            ev.getHeight());
             return false; });

        for (auto it = m_LayerStack.rbegin(); it != m_LayerStack.rend(); ++it)
        {
            (*it)->onEvent(e);
            if (e.Handled)
            {
                break;
            }
        }
    }

    Layer& HybridEngine::pushLayer(std::unique_ptr<Layer> layer)
    {
        Layer& result = m_LayerStack.pushLayer(std::move(layer));
        result.onAttach();
        return result;
    }

    void HybridEngine::requestExit() noexcept
    {
        m_Running = false;
        if (m_Window)
            m_Window->setShouldClose(true);
    }

    Layer& HybridEngine::pushOverlay(std::unique_ptr<Layer> layer)
    {
        Layer& result = m_LayerStack.pushOverlay(std::move(layer));
        result.onAttach();
        return result;
    }

    bool HybridEngine::consumePickResult(uint32_t &out_entity_id)
    {
        if (!m_HasPendingPickResult)
            return false;

        out_entity_id = m_LastPickResult;
        m_HasPendingPickResult = false;
        return true;
    }

    void HybridEngine::shutdown() noexcept
    {
        m_ExitRequestHandler = {};
        if (!m_Initialized && !m_Window && !m_RuntimeResourceSystem && !m_JobSystem)
            return;
        m_Running = false;
        HBD_CORE_INFO("{} shutdown_started", kEngineLogTag);

        m_LayerStack.clear();
        m_InputLayer.reset();

        if (m_RuntimeResourceSystem && m_RuntimeResourceSystem->getManager())
            m_RuntimeResourceSystem->getManager()->shutdown();
        if (m_JobSystem)
            m_JobSystem->waitIdle();

        m_PhysicsSystem.shutdown();
        m_RenderSystem.shutdown();
        m_ActiveScene.reset();
        m_SceneManager.setActiveScene(nullptr);
        m_RenderFrameRequest = {};
        m_RenderFrameResult = {};
        if (m_RuntimeResourceSystem)
        {
            m_RuntimeResourceSystem->shutdown();
            m_RuntimeResourceSystem.reset();
        }
        if (m_JobSystem)
        {
            m_JobSystem->shutdown();
            m_JobSystem.reset();
        }
        m_GraphicsContext.reset();

        if (m_Window)
        {
            m_Window->cleanup();
            m_Window.reset();
        }

        HBD_CORE_INFO("{} shutdown_completed", kEngineLogTag);
        LogSystem::shutdown();
        m_Initialized = false;
    }

    float HybridEngine::calculateDeltaTime()
    {
        const auto now = std::chrono::steady_clock::now();
        const float dt = std::chrono::duration<float>(now - m_LastFrameTime).count();
        m_LastFrameTime = now;
        return std::clamp(dt, 0.0f, 0.25f);
    }

    void HybridEngine::updateActiveScene(float dt)
    {
        if (!m_ActiveScene || !m_SceneUpdateEnabled)
            return;

        if (m_FixedUpdateEnabled)
        {
            const FrameAdvanceResult result = m_FrameClock.advance(dt, [this](float fixed_dt) {
                m_PhysicsSystem.tick(fixed_dt, *m_ActiveScene);
            });
            if (result.dropped_time)
                HBD_CORE_WARN("{} fixed_update_time_dropped max_steps=4", kEngineLogTag);
        }
        m_ActiveScene->onUpdate(dt);
    }

} // namespace Hybrid
