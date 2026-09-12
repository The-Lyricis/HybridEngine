#include "editor_app.h"

#include <filesystem>
#include <memory>
#include <string>
#include <utility>

#include "editor/core/engine_services.h"
#include "editor/framework/layers/editor_layer.h"
#include "editor/framework/layers/imgui_layer.h"
#include "editor/services/asset/editor_resource_system.h"
#include "editor/services/platform/editor_platform_services.h"
#include "editor/services/project/project_history.h"
#include "editor/services/project/project_instance_lock.h"
#include "editor/services/runtime/editor_session_controller.h"
#include "editor/services/render/editor_texture_service.h"
#include "editor/services/render/imgui_render_backend.h"
#include "runtime/core/base/macro.h"
#include "runtime/modules/project/project_creator.h"
#include "runtime/modules/project/project_paths.h"
#include "runtime/runtime/engine.h"

namespace Hybrid
{
    namespace
    {
        enum class ProjectLaunchSource
        {
            None = 0,
            ExplicitOpen,
            ExplicitCreate,
            RecentProject,
            FallbackProject
        };

        struct EditorLaunchRequest
        {
            std::filesystem::path project_path;
            std::filesystem::path new_project_root;
            std::string new_project_name;
            GraphicsBackend render_backend = GraphicsBackend::OpenGL;
            bool allow_render_fallback = false;
            std::string argument_error;
        };

        EditorLaunchRequest parseLaunchRequest(int argc, char** argv)
        {
            EditorLaunchRequest request{};
            for (int i = 1; i < argc; ++i)
            {
                const std::string arg = argv[i] ? argv[i] : "";
                if (arg.empty())
                    continue;

                constexpr const char* render_api_prefix = "--render-api=";
                if (arg.rfind(render_api_prefix, 0) == 0)
                {
                    const std::string value = arg.substr(std::char_traits<char>::length(render_api_prefix));
                    const auto backend = ParseGraphicsBackend(value);
                    if (!backend)
                    {
                        request.argument_error = "invalid --render-api value: " + value;
                        break;
                    }
                    request.render_backend = *backend;
                    continue;
                }

                if (arg == "--project" || arg == "-p")
                {
                    if (i + 1 < argc && argv[i + 1] != nullptr)
                        request.project_path = std::filesystem::path(argv[++i]);
                    continue;
                }

                if (arg == "--new-project")
                {
                    if (i + 1 < argc && argv[i + 1] != nullptr)
                        request.new_project_root = std::filesystem::path(argv[++i]);
                    continue;
                }

                if (arg == "--project-name")
                {
                    if (i + 1 < argc && argv[i + 1] != nullptr)
                        request.new_project_name = argv[++i];
                    continue;
                }

                if (arg == "--render-api")
                {
                    if (i + 1 >= argc || argv[i + 1] == nullptr)
                    {
                        request.argument_error = "missing value after --render-api";
                        break;
                    }
                    const std::string value = argv[++i];
                    const auto backend = ParseGraphicsBackend(value);
                    if (!backend)
                    {
                        request.argument_error = "invalid --render-api value: " + value;
                        break;
                    }
                    request.render_backend = *backend;
                    continue;
                }

                if (arg == "--allow-render-fallback")
                {
                    request.allow_render_fallback = true;
                    continue;
                }

                if (!arg.empty() && arg[0] == '-')
                    continue;

                if (request.project_path.empty())
                    request.project_path = std::filesystem::path(arg);
            }

            return request;
        }

        struct ProjectLaunchDecision
        {
            ProjectLaunchSource source = ProjectLaunchSource::None;
            std::filesystem::path requested_path;
            std::filesystem::path resolved_project_file;
            bool should_persist_recent = false;
        };

        const char* toString(ProjectLaunchSource source)
        {
            switch (source)
            {
            case ProjectLaunchSource::ExplicitOpen:   return "explicit_open";
            case ProjectLaunchSource::ExplicitCreate: return "explicit_create";
            case ProjectLaunchSource::RecentProject:  return "recent_project";
            case ProjectLaunchSource::FallbackProject:return "fallback_project";
            default:                                  return "none";
            }
        }

        bool decideProjectLaunch(const EditorLaunchRequest& request,
                                 const IEditorPlatformServices& platform,
                                 ProjectLaunchDecision& out_decision,
                                 std::string& out_error)
        {
            namespace fs = std::filesystem;
            out_decision = {};

            if (!request.new_project_root.empty() && !request.project_path.empty())
            {
                out_error = "--project and --new-project cannot be used together";
                return false;
            }

            if (!request.project_path.empty())
            {
                out_decision.source = ProjectLaunchSource::ExplicitOpen;
                out_decision.requested_path = request.project_path;
                out_decision.should_persist_recent = true;
                if (!ResolveProjectFilePath(request.project_path, out_decision.resolved_project_file, out_error))
                    return false;
                return true;
            }

            if (!request.new_project_root.empty())
            {
                const fs::path project_root = fs::absolute(request.new_project_root);
                std::string project_name = request.new_project_name;
                if (project_name.empty())
                {
                    project_name = project_root.filename().string();
                    if (project_name.empty())
                        project_name = "NewProject";
                }

                ProjectCreateDesc desc{};
                desc.project_root = project_root;
                desc.project_name = project_name;

                out_decision.source = ProjectLaunchSource::ExplicitCreate;
                out_decision.requested_path = project_root;
                out_decision.should_persist_recent = true;
                return ProjectCreator::CreateProject(desc, out_decision.resolved_project_file, out_error);
            }

            RecentProjectState recent_state{};
            if (ProjectHistory::loadRecentState(platform, recent_state) &&
                !recent_state.recent_project_files.empty())
            {
                std::string resolve_error;
                if (ResolveProjectFilePath(recent_state.recent_project_files.front(),
                                           out_decision.resolved_project_file,
                                           resolve_error))
                {
                    out_decision.source = ProjectLaunchSource::RecentProject;
                    out_decision.requested_path = recent_state.recent_project_files.front();
                    out_decision.should_persist_recent = true;
                    return true;
                }
            }

            out_decision.source = ProjectLaunchSource::FallbackProject;
            const ProjectCreateDesc bootstrap_desc =
                MakeDebugBootstrapProjectDesc(fs::path(HYBRID_ROOT_DIR));
            out_decision.requested_path = bootstrap_desc.project_root;
            if (!ProjectCreator::CreateProject(bootstrap_desc, out_decision.resolved_project_file, out_error))
                return false;
            out_decision.should_persist_recent = false;
            return true;
        }
    }

    int EditorApp::run(int argc, char** argv)
    {
        constexpr const char* kEditorAppLogTag = "[EditorApp]";
        auto platform_services = CreateEditorPlatformServices();
        if (!platform_services)
        {
            HBD_CORE_ERROR("{} startup_failed reason=platform_services_unavailable", kEditorAppLogTag);
            return 1;
        }
        const EditorLaunchRequest launch_request = parseLaunchRequest(argc, argv);
        if (!launch_request.argument_error.empty())
        {
            HBD_CORE_ERROR("{} launch_argument_failed reason={}", kEditorAppLogTag, launch_request.argument_error);
            return 1;
        }
        ProjectLaunchDecision launch_decision{};
        std::string project_error;
        if (!decideProjectLaunch(launch_request, *platform_services, launch_decision, project_error))
        {
            HBD_CORE_ERROR("{} launch_request_failed reason={}", kEditorAppLogTag, project_error);
            return 1;
        }

        HBD_CORE_INFO("{} launch_decision source={} requested_path={} resolved_project={}",
                      kEditorAppLogTag,
                      toString(launch_decision.source),
                      launch_decision.requested_path.generic_string(),
                      launch_decision.resolved_project_file.generic_string());

        ProjectInstanceLock project_lock;
        if (!launch_decision.resolved_project_file.empty())
        {
            std::string lock_error;
            if (!project_lock.acquire(launch_decision.resolved_project_file, lock_error))
            {
                HBD_CORE_ERROR("{} project_instance_lock_failed project={} reason={}",
                               kEditorAppLogTag,
                               launch_decision.resolved_project_file.generic_string(),
                               lock_error);
                return 1;
            }
        }

        HybridEngine engine;
        EngineConfig engine_config{};
        engine_config.project_path = launch_decision.resolved_project_file;
        engine_config.render_backend = launch_request.render_backend;
        engine_config.allow_render_fallback = launch_request.allow_render_fallback;
        if (!engine.initialize(engine_config))
        {
            project_lock.release();
            return 1;
        }
        platform_services->configureApplicationAppearance();
        HBD_CORE_INFO("{} engine_initialized", kEditorAppLogTag);
        if (!launch_decision.resolved_project_file.empty())
        {
            HBD_CORE_INFO("{} project_path_requested path={}",
                          kEditorAppLogTag,
                          launch_decision.resolved_project_file.generic_string());
        }

        if (launch_decision.should_persist_recent && !launch_decision.resolved_project_file.empty())
        {
            if (!ProjectHistory::addRecentProject(*platform_services, launch_decision.resolved_project_file))
            {
                HBD_CORE_WARN("{} recent_project_save_failed path={}",
                              kEditorAppLogTag,
                              launch_decision.resolved_project_file.generic_string());
            }
        }

        HBD_CORE_INFO("{} run_started", kEditorAppLogTag);

        auto editor_resources = std::make_shared<EditorResourceSystem>();
        if (!editor_resources->initialize(engine.getResourceSystem(), engine.getJobSystem()))
        {
            HBD_CORE_ERROR("{} editor_resources_initialize_failed", kEditorAppLogTag);
            engine.shutdown();
            return 1;
        }
        auto imgui_backend = std::shared_ptr<IImGuiRenderBackend>(
            CreateImGuiRenderBackend(engine.getWindowSystem().graphicsBackend()));
        if (!imgui_backend)
        {
            HBD_CORE_ERROR("{} imgui_backend_unavailable backend={}",
                           kEditorAppLogTag, ToString(engine.getWindowSystem().graphicsBackend()));
            editor_resources->shutdown();
            engine.shutdown();
            return 1;
        }
        auto editor_textures = std::make_shared<EditorTextureService>(imgui_backend);

        EditorSessionController session(engine);
        EngineServices services{};
        services.window = &engine.getWindowSystem();
        services.render = &engine.getRenderSystem();
        services.scene = &engine.getSceneManager();
        services.resources = &engine.getResourceSystem();
        services.editor_resources = editor_resources.get();
        services.editor_textures = editor_textures.get();
        services.platform = platform_services.get();
        services.input = &engine.getInputLayer();
        services.frame_context = &engine.getFrameContext();
        services.render_flags = &engine.getRenderFlags();
        services.render_request = &engine.getRenderFrameRequest();
        services.render_result = &engine.getRenderFrameResult();
        services.jobs = engine.getJobSystem();
        services.request_exit = [&engine]() { engine.requestExit(); };
        services.consume_pick_result = [&engine](uint32_t& id) { return engine.consumePickResult(id); };
        services.set_editor_scene = [&session](std::shared_ptr<Scene> scene) -> bool
            {
                return session.setEditorScene(std::move(scene));
            };

        engine.pushOverlay(std::make_unique<ImGuiLayer>(engine.getWindowSystem(), imgui_backend));
        auto editor_layer = std::make_unique<EditorLayer>(std::move(services));
        auto* editor_layer_ptr = editor_layer.get();

        EditorModeCallbacks mode_callbacks;
        mode_callbacks.enter_play_mode_from_scene = [&session](std::shared_ptr<Scene> scene) -> bool
            {
                return session.enterPlayModeFromScene(scene);
            };
        mode_callbacks.exit_play_mode = [&session]()
            {
                session.exitPlayMode();
            };
        mode_callbacks.toggle_pause_mode = [&session]()
            {
                session.togglePause();
            };
        mode_callbacks.is_play_mode = [&session]() -> bool
            {
                return session.isPlayMode();
            };
        mode_callbacks.is_pause_mode = [&session]() -> bool
            {
                return session.isPaused();
            };
        editor_layer_ptr->setModeCallbacks(std::move(mode_callbacks));

        engine.pushLayer(std::move(editor_layer));
        engine.setExitRequestHandler([editor_layer_ptr]() { editor_layer_ptr->requestExit(); });
        HBD_CORE_INFO("{} layers_attached", kEditorAppLogTag);

        engine.run();
        HBD_CORE_INFO("{} run_completed exit_code=0", kEditorAppLogTag);
        engine.shutdown();
        editor_resources->shutdown();
        project_lock.release();
        return 0;
    }
} // namespace Hybrid
