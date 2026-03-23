#include "editor_app.h"

#include <filesystem>
#include <memory>
#include <string>
#include <utility>

#include "editor/core/engine_services.h"
#include "editor/framework/layers/editor_layer.h"
#include "editor/framework/layers/imgui_layer.h"
#include "editor/platform/windows/editor_platform_services_win32.h"
#include "editor/services/asset/editor_resource_system.h"
#include "runtime/core/base/macro.h"
#include "runtime/modules/project/project_creator.h"
#include "runtime/runtime/engine.h"

namespace Hybrid
{
    namespace
    {
        struct EditorLaunchRequest
        {
            std::filesystem::path project_path;
            std::filesystem::path new_project_root;
            std::string new_project_name;
        };

        EditorLaunchRequest parseLaunchRequest(int argc, char** argv)
        {
            EditorLaunchRequest request{};
            for (int i = 1; i < argc; ++i)
            {
                const std::string arg = argv[i] ? argv[i] : "";
                if (arg.empty())
                    continue;

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

                if (!arg.empty() && arg[0] == '-')
                    continue;

                if (request.project_path.empty())
                    request.project_path = std::filesystem::path(arg);
            }

            return request;
        }

        bool resolveEditorProjectPath(const EditorLaunchRequest& request,
                                      std::filesystem::path& out_project_path,
                                      std::string& out_error)
        {
            namespace fs = std::filesystem;

            if (!request.new_project_root.empty() && !request.project_path.empty())
            {
                out_error = "--project and --new-project cannot be used together";
                return false;
            }

            if (request.new_project_root.empty())
            {
                out_project_path = request.project_path;
                return true;
            }

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
            return ProjectCreator::CreateProject(desc, out_project_path, out_error);
        }
    }

    int EditorApp::run(int argc, char** argv)
    {
        constexpr const char* kEditorAppLogTag = "[EditorApp]";
        const EditorLaunchRequest launch_request = parseLaunchRequest(argc, argv);
        std::filesystem::path project_path;
        std::string project_error;
        if (!resolveEditorProjectPath(launch_request, project_path, project_error))
        {
            HBD_CORE_ERROR("{} launch_request_failed reason={}", kEditorAppLogTag, project_error);
            return 1;
        }

        HybridEngine engine;
        if (!engine.initialize(project_path))
            return 1;
        HBD_CORE_INFO("{} engine_initialized", kEditorAppLogTag);
        if (!project_path.empty())
        {
            HBD_CORE_INFO("{} project_path_requested path={}",
                          kEditorAppLogTag,
                          project_path.generic_string());
        }
        HBD_CORE_INFO("{} run_started", kEditorAppLogTag);

        auto editor_resources = std::make_shared<EditorResourceSystem>();
        auto platform_services = std::make_unique<EditorPlatformServicesWin32>();
        if (!editor_resources->initialize(engine.getResourceSystem()))
        {
            HBD_CORE_ERROR("{} editor_resources_initialize_failed", kEditorAppLogTag);
            engine.shutdown();
            return 1;
        }

        EngineServices services{};
        services.window = &engine.getWindowSystem();
        services.render = &engine.getRenderSystem();
        services.scene = &engine.getSceneManager();
        services.resources = &engine.getResourceSystem();
        services.editor_resources = editor_resources.get();
        services.platform = platform_services.get();
        services.input = &engine.getInputLayer();
        services.frame_context = &engine.getFrameContext();
        services.render_flags = &engine.getRenderFlags();
        services.editor_ext = &engine.getEditorRenderExt();
        services.consume_pick_result = [&engine](uint32_t& id) { return engine.consumePickResult(id); };
        services.set_editor_scene = [&engine](std::shared_ptr<Scene> scene) -> bool
            {
                return engine.setEditorScene(std::move(scene));
            };

        engine.pushOverlay(new ImGuiLayer(engine.getWindowSystem().getNativeWindow()));
        auto* editor_layer = new EditorLayer(std::move(services));

        EditorModeCallbacks mode_callbacks;
        mode_callbacks.enter_play_mode_from_scene = [&engine](std::shared_ptr<Scene> scene) -> bool
            {
                return engine.enterPlayModeFromScene(scene);
            };
        mode_callbacks.exit_play_mode = [&engine]()
            {
                engine.exitPlayMode();
            };
        mode_callbacks.toggle_pause_mode = [&engine]()
            {
                engine.togglePlayPause();
            };
        mode_callbacks.is_play_mode = [&engine]() -> bool
            {
                return engine.isPlayMode();
            };
        mode_callbacks.is_pause_mode = [&engine]() -> bool
            {
                return engine.isPlayPaused();
            };
        editor_layer->setModeCallbacks(std::move(mode_callbacks));

        engine.pushLayer(editor_layer);
        HBD_CORE_INFO("{} layers_attached", kEditorAppLogTag);

        engine.run();
        HBD_CORE_INFO("{} run_completed exit_code=0", kEditorAppLogTag);
        engine.shutdown();
        return 0;
    }
} // namespace Hybrid

