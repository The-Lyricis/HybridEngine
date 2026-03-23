#include "engine.h"

#include <algorithm>
#include <string>
#include <vector>

#include "runtime/core/base/macro.h"
#include "runtime/core/base/math_util.h"
#include "runtime/core/log/log_system.h"

#include <filesystem>
#include "runtime/modules/project/project_creator.h"
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

        ProjectCreateDesc makeDebugBootstrapProjectDesc(const std::filesystem::path& output_dir)
        {
            ProjectCreateDesc desc{};
            desc.project_root = output_dir / "GameProject";
            desc.project_name = "GameProject";
            return desc;
        }

        bool resolveProjectFilePath(const std::filesystem::path& requested_path, std::filesystem::path& out_hyproj_path, std::string& out_error)
        {
            namespace fs = std::filesystem;

            if (requested_path.empty())
            {
                out_error = "project path is empty";
                return false;
            }

            const fs::path absolute_path = fs::absolute(requested_path);
            if (fs::is_regular_file(absolute_path))
            {
                if (absolute_path.extension() != ".hyproj")
                {
                    out_error = "project file must use the .hyproj extension: " + absolute_path.string();
                    return false;
                }

                out_hyproj_path = absolute_path;
                return true;
            }

            if (fs::is_directory(absolute_path))
            {
                std::vector<fs::path> hyproj_files;
                std::error_code ec;
                for (const auto& entry : fs::directory_iterator(absolute_path, ec))
                {
                    if (ec)
                    {
                        out_error = "failed to enumerate project directory: " + absolute_path.string() + " (" + ec.message() + ")";
                        return false;
                    }

                    if (entry.is_regular_file() && entry.path().extension() == ".hyproj")
                        hyproj_files.push_back(entry.path());
                }

                if (hyproj_files.empty())
                {
                    out_error = "no .hyproj file found in directory: " + absolute_path.string();
                    return false;
                }

                std::sort(hyproj_files.begin(), hyproj_files.end());
                out_hyproj_path = hyproj_files.front();
                return true;
            }

            out_error = "project path does not exist: " + absolute_path.string();
            return false;
        }
    } // namespace

    bool HybridEngine::initialize(const std::filesystem::path& project_path)
    {
        LogSystem::initialize();

        namespace fs = std::filesystem;
        const fs::path cwd = fs::current_path();
        fs::path hyproj_path;
        if (project_path.empty())
        {
            const ProjectCreateDesc bootstrap_desc = makeDebugBootstrapProjectDesc(cwd);
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
            if (!resolveProjectFilePath(project_path, hyproj_path, resolve_error))
            {
                HBD_CORE_ERROR("{} project_path_resolve_failed requested_path={} reason={}",
                               kEngineLogTag,
                               pathOrPlaceholder(project_path),
                               resolve_error);
                LogSystem::shutdown();
                return false;
            }

            HBD_CORE_INFO("{} initialize_started cwd={} requested_project={} resolved_hyproj={}",
                          kEngineLogTag,
                          pathOrPlaceholder(cwd),
                          pathOrPlaceholder(project_path),
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

        // ===== Window / Graphics =====
        m_Window = std::make_shared<WindowSystem>();
        m_Window->initialize(1280, 720, "Hybrid Engine");

        GLFWwindow *window = m_Window->getNativeWindow();
        if (!window)
        {
            HBD_CORE_ERROR("{} initialize_failed step=window_create reason=native_window_null",
                           kEngineLogTag);
            m_Window->cleanup();
            LogSystem::shutdown();
            return false;
        }

        m_GraphicsContext = GraphicsContext::Create(window);
        if (!m_GraphicsContext)
        {
            HBD_CORE_ERROR("{} initialize_failed step=graphics_context_create reason=create_failed",
                           kEngineLogTag);
            m_Window->cleanup();
            LogSystem::shutdown();
            return false;
        }
        m_GraphicsContext->init();

        // ===== Resource System (Project-based) =====
        m_RuntimeResourceSystem = std::make_shared<RuntimeResourceSystem>();

        // Pass nullptr VFS to let RuntimeResourceSystem create the default NativeFileSystem.
        m_RuntimeResourceSystem->initialize(Hybrid::ProjectService::Get(), nullptr);

        m_RenderSystem.setAssetManager(m_RuntimeResourceSystem->getManager());

        // ===== Event / Layers =====
        auto surface_io = m_Window->getSurfaceIO();
        surface_io->registerOnEventFunc([this](Event &e)
                                        { onEvent(e); });

        m_InputLayer = new InputLayer();
        pushLayer(m_InputLayer);

        // ===== Render =====
        m_RenderSystem.initialize(window);

        // ===== Scene =====
        auto scene = std::make_shared<Scene>();
        scene->setName("Untitled");
        m_EditorScene = scene;
        m_SceneManager.setActiveScene(scene);
        m_RenderSystem.setScene(scene);
        m_FrameContext.scene = scene;
        m_FrameContext.window_handle = window;

        // 绑定到系统
        if (!setEditorScene(scene))
        {
            HBD_CORE_ERROR("{} initialize_failed step=editor_scene_bind scene={} reason=set_editor_scene_failed",
                           kEngineLogTag,
                           sceneNameOrPlaceholder(scene));
            m_Window->cleanup();
            LogSystem::shutdown();
            return false;
        }
        m_FrameContext.window_handle = window;

        int fbw = 0, fbh = 0;
        glfwGetFramebufferSize(window, &fbw, &fbh);
        m_FrameContext.viewport_size.x = static_cast<float>(std::max(1, fbw));
        m_FrameContext.viewport_size.y = static_cast<float>(std::max(1, fbh));

        m_LastTime = static_cast<float>(glfwGetTime());

        //物理系统初始化
        m_PhysicsSystem.initialize();
        HBD_CORE_INFO("{} initialize_completed scene={} viewport={}x{} mode=edit",
                      kEngineLogTag,
                      sceneNameOrPlaceholder(m_EditorScene),
                      static_cast<int>(m_FrameContext.viewport_size.x),
                      static_cast<int>(m_FrameContext.viewport_size.y));
        return true;
    }

    bool HybridEngine::setEditorScene(std::shared_ptr<Scene> scene)
    {
        if (!scene)
        {
            HBD_CORE_WARN("{} editor_scene_set_rejected reason=null_scene", kEngineLogTag);
            return false;
        }

        if (isPlayMode())
            exitPlayMode();

        m_HasPendingPickResult = false;
        m_LastPickResult = kInvalidEntityID;
        m_EditorRenderExt.request_pick = false;
        m_EditorRenderExt.selection.selected_entities.clear();
        m_EditorRenderExt.selection.active_entity = kInvalidEntityID;
        m_EditorRenderExt.selection.hovered_entity = kInvalidEntityID;

        m_EditorScene = std::move(scene);
        m_SceneManager.setActiveScene(m_EditorScene);
        m_RenderSystem.setScene(m_EditorScene);

        if (m_SceneRunState == SceneRunState::Edit)
            m_FrameContext.scene = m_EditorScene;

        const bool success = (m_EditorScene != nullptr) && (m_SceneManager.getActiveScene() == m_EditorScene);
        if (success)
        {
            HBD_CORE_DEBUG("{} editor_scene_set scene={} mode={}",
                           kEngineLogTag,
                           sceneNameOrPlaceholder(m_EditorScene),
                           isPlayMode() ? "play" : "edit");
        }
        else
        {
            HBD_CORE_ERROR("{} editor_scene_set_failed scene={} reason=activation_mismatch",
                           kEngineLogTag,
                           sceneNameOrPlaceholder(m_EditorScene));
        }

        return success;
    }

    void HybridEngine::run()
    {
        HBD_CORE_INFO("{} run_started mode={}", kEngineLogTag, isPlayMode() ? "play" : "edit");
        while (m_Running && !m_Window->shouldClose())
        {
            const float dt = calculateDeltaTime();
            m_Window->pollEvents();

            for (Layer *layer : m_LayerStack)
            {
                layer->onBeginFrame();
            }

            if (m_Minimized)
            {
                for (auto it = m_LayerStack.rbegin(); it != m_LayerStack.rend(); ++it)
                {
                    (*it)->onEndFrame();
                }
                m_GraphicsContext->swapBuffers();
                continue;
            }

            for (Layer *layer : m_LayerStack)
            {
                layer->onUpdate(dt);
            }

            if (isPlayMode())
            {
                updatePlayMode(dt);
            }
            else
            {
                updateEditMode(dt);
            }

            m_FrameContext.dt = dt;
            m_FrameContext.input = &m_InputLayer->getState();
            m_FrameContext.scene = getActiveGameScene();
            m_RenderSystem.update(dt);

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

            for (Layer *layer : m_LayerStack)
            {
                layer->onImGuiRender();
            }

            if (m_EditorRenderExt.request_pick && HasFlag(m_RenderFlags, RenderFlags::PickingID))
            {
                m_LastPickResult = m_RenderSystem.readEntityID(m_EditorRenderExt.pick_x, m_EditorRenderExt.pick_y);
                m_HasPendingPickResult = true;
                m_EditorRenderExt.request_pick = false;
            }

            for (auto it = m_LayerStack.rbegin(); it != m_LayerStack.rend(); ++it)
            {
                (*it)->onEndFrame();
            }

            m_GraphicsContext->swapBuffers();
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
            m_Running = false;
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

    void HybridEngine::pushLayer(Layer *layer)
    {
        if (!layer)
            return;
        m_LayerStack.pushLayer(layer);
        layer->onAttach();
    }

    void HybridEngine::pushOverlay(Layer *layer)
    {
        if (!layer)
            return;
        m_LayerStack.pushOverlay(layer);
        layer->onAttach();
    }

    bool HybridEngine::consumePickResult(uint32_t &out_entity_id)
    {
        if (!m_HasPendingPickResult)
            return false;

        out_entity_id = m_LastPickResult;
        m_HasPendingPickResult = false;
        return true;
    }

    void HybridEngine::shutdown()
    {
        HBD_CORE_INFO("{} shutdown_started mode={}", kEngineLogTag, isPlayMode() ? "play" : "edit");
        if (isPlayMode())
            exitPlayMode();

        m_PhysicsSystem.shutdown();
        m_LayerStack.clear();
        m_InputLayer = nullptr;

        if (m_Window)
        {
            m_Window->cleanup();
        }

        HBD_CORE_INFO("{} shutdown_completed", kEngineLogTag);
        LogSystem::shutdown();
    }

    float HybridEngine::calculateDeltaTime()
    {
        const float time = static_cast<float>(glfwGetTime());
        const float dt = time - m_LastTime;
        m_LastTime = time;
        return dt;
    }

    void HybridEngine::updateEditMode(float dt)
    {
        if (m_EditorScene)
        {
            m_EditorScene->onUpdate(dt);
        }
    }

    void HybridEngine::updatePlayMode(float dt)
    {
        if (!m_RuntimeScene)
            return;

        if (m_PlayPaused)
            return;

        m_PhysicsSystem.tick(dt, *m_RuntimeScene);
        m_RuntimeScene->onUpdate(dt);
    }

    void HybridEngine::enterPlayMode()
    {
        (void)enterPlayModeFromScene(m_EditorScene);
    }

    bool HybridEngine::enterPlayModeFromScene(const std::shared_ptr<Scene>& source_scene)
    {
        if (isPlayMode())
        {
            HBD_CORE_DEBUG("{} play_mode_enter_skipped reason=already_in_play_mode", kEngineLogTag);
            return false;
        }

        if (!source_scene)
        {
            HBD_CORE_WARN("{} play_mode_enter_rejected reason=null_source_scene", kEngineLogTag);
            return false;
        }

        m_RuntimeScene = source_scene->cloneRuntime();
        if (!m_RuntimeScene)
        {
            HBD_CORE_ERROR("{} play_mode_enter_failed scene={} reason=clone_runtime_failed",
                           kEngineLogTag,
                           sceneNameOrPlaceholder(source_scene));
            return false;
        }

        m_SceneRunState = SceneRunState::Play;
        m_PlayPaused = false;
        m_SceneManager.setActiveScene(m_RuntimeScene);
        m_RenderSystem.setScene(m_RuntimeScene);
        m_FrameContext.scene = m_RuntimeScene;
        HBD_CORE_INFO("{} play_mode_entered source_scene={} runtime_scene={}",
                      kEngineLogTag,
                      sceneNameOrPlaceholder(source_scene),
                      sceneNameOrPlaceholder(m_RuntimeScene));
        return true;
    }

    void HybridEngine::exitPlayMode()
    {
        if (isEditMode())
        {
            HBD_CORE_DEBUG("{} play_mode_exit_skipped reason=already_in_edit_mode", kEngineLogTag);
            return;
        }

        const std::string runtime_scene_name = sceneNameOrPlaceholder(m_RuntimeScene);
        m_RuntimeScene.reset();
        m_SceneRunState = SceneRunState::Edit;
        m_PlayPaused = false;
        m_SceneManager.setActiveScene(m_EditorScene);
        m_RenderSystem.setScene(m_EditorScene);
        m_FrameContext.scene = m_EditorScene;
        HBD_CORE_INFO("{} play_mode_exited runtime_scene={} editor_scene={}",
                      kEngineLogTag,
                      runtime_scene_name,
                      sceneNameOrPlaceholder(m_EditorScene));
    }

    void HybridEngine::togglePlayPause()
    {
        if (!isPlayMode())
            return;

        m_PlayPaused = !m_PlayPaused;
        HBD_CORE_INFO("{} play_mode_pause_toggled paused={} scene={}",
                      kEngineLogTag,
                      m_PlayPaused ? "true" : "false",
                      sceneNameOrPlaceholder(m_RuntimeScene));
    }

} // namespace Hybrid
