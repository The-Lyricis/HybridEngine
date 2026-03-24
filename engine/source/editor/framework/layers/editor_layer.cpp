#include "editor_layer.h"

#include <algorithm>
#include <cmath>
#include <entt/entity/entity.hpp>
#include <glm/glm.hpp>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "editor/core/context/editor_context.h"
#include "editor/core/commands/entity_commands.h"
#include "editor/core/snapshot/entity_snapshot.h"
#include "editor/core/snapshot/selection_snapshot.h"
#include "editor/core/commands/transform_command.h"
#include "editor/core/snapshot/transform_snapshot.h"
#include "editor/services/import/import_types.h"
#include "editor/services/platform/editor_platform_services.h"
#include "editor/services/project/project_history.h"
#include "editor/services/project/project_instance_lock.h"
#include "runtime/core/base/intersection.h"
#include "runtime/core/base/macro.h"
#include "runtime/modules/asset/asset_registry.h"
#include "runtime/modules/asset/asset_type.h"
#include "runtime/modules/asset/mesh.h"
#include "runtime/modules/asset/runtime_resource_system.h"
#include "runtime/modules/input/input_layer.h"
#include "runtime/modules/project/project_context.h"
#include "runtime/modules/project/project_paths.h"
#include "runtime/modules/render/runtime/editor_render_ext.h"
#include "runtime/modules/render/runtime/frame_context.h"
#include "runtime/modules/render/runtime/render_flags.h"
#include "runtime/modules/render/runtime/render_system.h"
#include "runtime/modules/scene/components.h"
#include "runtime/modules/scene/scene.h"
#include "runtime/modules/scene/scene_manager.h"
#include "runtime/modules/window/window_system.h"

namespace Hybrid
{
    namespace
    {
        constexpr const char* kEditorLayerLogTag = "[EditorLayer]";

        std::string displayNameForAssetMeta(const AssetMetadata* meta)
        {
            if (meta == nullptr)
                return "None";

            if (!meta->source_path.empty())
            {
                const std::filesystem::path source_path(meta->source_path);
                const std::string filename = source_path.filename().string();
                if (!filename.empty())
                    return filename;
                return meta->source_path;
            }

            return std::to_string(meta->id.value);
        }
    }

    EditorLayer::EditorLayer(EngineServices services)
        : Layer("EditorLayer")
        , m_services(services)
        , m_asset_hot_reload_controller(services)
        , m_scene_io(services)
    {
    }

    void EditorLayer::onAttach()
    {
        if (!m_services.window || !m_services.render || !m_services.scene ||
            !m_services.frame_context || !m_services.render_flags || !m_services.editor_ext)
        {
            HBD_CORE_ERROR("{} attach_failed reason=missing_required_engine_services", kEditorLayerLogTag);
            return;
        }

        m_editor_ui.initialize(m_services.window->getNativeWindow());
        m_asset_hot_reload_controller.initialize([this](const std::string& message) {
            m_editor_ui.context().setStatusMessage(message);
        });
        m_scene_io.initialize();
        m_initialized = true;

        auto& ctx = m_editor_ui.context();
        ctx.debug.render_stats = &m_services.render->getStats();
        BindEditorContextActions(ctx, buildContextActionBindings());
        m_asset_hot_reload_controller.bindContext(ctx);

        (void)m_scene_io.restoreStartupScene();
        syncContextDocumentState();
        HBD_CORE_INFO("{} attach_completed", kEditorLayerLogTag);
    }

    void EditorLayer::onDetach()
    {
        if (!m_initialized)
            return;

        if (m_active_scene_view_document)
            (void)m_scene_io.saveSceneViewState(*m_active_scene_view_document, m_editor_camera.dumpState());

        auto& ctx = m_editor_ui.context();
        ClearEditorContextActions(ctx);
        ctx.selection.clear();
        ctx.clearActiveDocument();
        m_asset_hot_reload_controller.unbindContext(ctx);

        if (m_services.editor_ext)
            m_services.editor_ext->has_editor_camera = false;

        m_asset_hot_reload_controller.shutdown();
        m_scene_io.shutdown();
        m_editor_ui.shutdown();
        m_active_scene_view_document.reset();
        m_initialized = false;
        HBD_CORE_INFO("{} detach_completed", kEditorLayerLogTag);
    }

    EditorCommandContext EditorLayer::makeCommandContext()
    {
        return EditorCommandContext{&m_editor_ui.context()};
    }

    bool EditorLayer::executeCommand(EditorCommandId id)
    {
        auto ctx = makeCommandContext();
        return m_command_dispatcher.execute(id, ctx);
    }

    bool EditorLayer::canExecuteCommand(EditorCommandId id) const
    {
        auto& ui_ctx = m_editor_ui.context();
        return m_command_dispatcher.canExecute(id, EditorCommandContext{const_cast<EditorContext*>(&ui_ctx)});
    }

    EditorDocumentActions EditorLayer::buildDocumentActions()
    {
        EditorDocumentActions actions{};
        actions.open_scene = [this](const std::string& scene_vpath)
        {
            const bool opened = m_scene_io.open(scene_vpath);
            if (opened)
                m_editor_ui.context().selection.clear();
            syncContextDocumentState();
        };
        actions.request_open_project = [this]() -> bool
        {
            return requestOpenProject();
        };
        actions.request_open_recent_project = [this](const std::filesystem::path& project_path) -> bool
        {
            return openProjectInNewInstance(project_path);
        };
        actions.list_recent_projects = [this]() -> std::vector<std::filesystem::path>
        {
            if (!m_services.platform)
                return {};

            RecentProjectState state{};
            if (!ProjectHistory::loadRecentState(*m_services.platform, state))
                return {};
            return state.recent_project_files;
        };
        actions.request_open_scene = [this]() -> bool
        {
            const bool opened = m_scene_io.requestOpen();
            if (opened)
                m_editor_ui.context().selection.clear();
            syncContextDocumentState();
            return opened;
        };
        actions.request_new_scene = [this]() -> bool
        {
            const bool created = m_scene_io.createUntitled();
            if (created)
                m_editor_ui.context().selection.clear();
            syncContextDocumentState();
            return created;
        };
        actions.request_reset_layout = [this]()
        {
            m_editor_ui.requestResetLayout();
        };
        actions.request_save_scene = [this]() -> bool
        {
            const bool saved = m_scene_io.requestSave();
            syncContextDocumentState();
            return saved;
        };
        actions.request_save_scene_as = [this]() -> bool
        {
            const bool saved = m_scene_io.requestSaveAs();
            syncContextDocumentState();
            return saved;
        };
        actions.request_confirm_dialog = [this](EditorConfirmDialog dialog)
        {
            m_editor_ui.queueConfirmDialog(std::move(dialog));
        };
        actions.reveal_in_file_browser = [this](const std::filesystem::path& path) -> bool
        {
            return m_services.platform ? m_services.platform->revealInFileBrowser(path) : false;
        };
        return actions;
    }

    EditorSceneActions EditorLayer::buildSceneActions()
    {
        EditorSceneActions actions{};
        actions.create_entity = [this](SceneEntityTemplate type, entt::entity parent) -> entt::entity
        {
            return createSceneEntity(type, parent);
        };
        actions.delete_entity = [this](entt::entity entity_handle) -> bool
        {
            return deleteSceneEntity(entity_handle);
        };
        actions.duplicate_selection = [this](entt::entity target_entity) -> bool
        {
            return duplicateSceneSelection(target_entity);
        };
        actions.instantiate_asset = [this](AssetID asset_id, const ImVec2& drop_mouse_pos) -> bool
        {
            const bool instantiated = instantiateSceneAsset(asset_id, drop_mouse_pos);
            syncContextDocumentState();
            return instantiated;
        };
        actions.instantiate_project_path = [this](const std::string& rel_path, const ImVec2& drop_mouse_pos) -> bool
        {
            const bool instantiated = instantiateSceneProjectPath(rel_path, drop_mouse_pos);
            syncContextDocumentState();
            return instantiated;
        };
        actions.fit_box_collider_to_mesh = [this](entt::entity entity_handle) -> bool
        {
            const bool fitted = fitBoxColliderToMesh(entity_handle);
            syncContextDocumentState();
            return fitted;
        };
        actions.get_builtin_mesh_id = [this](BuiltinMesh mesh) -> AssetID
        {
            if (!m_services.resources)
                return {};
            return m_services.resources->getBuiltinMeshID(mesh);
        };
        return actions;
    }

    EditorAssetActions EditorLayer::buildAssetActions()
    {
        EditorAssetActions actions{};
        actions.request_reimport_asset = [this](const std::string& asset_vpath) -> bool
        {
            const bool ok = m_asset_hot_reload_controller.requestReimport(asset_vpath);
            syncContextDocumentState();
            return ok;
        };
        actions.request_rename_folder = [this](const std::string& old_folder_vpath, const std::string& new_folder_vpath) -> bool
        {
            const bool ok = m_asset_hot_reload_controller.requestRenameFolder(old_folder_vpath, new_folder_vpath);
            syncContextDocumentState();
            return ok;
        };
        actions.find_asset_by_vpath = [this](const std::string& asset_vpath) -> AssetID
        {
            return findAssetByVPath(asset_vpath);
        };
        actions.describe_mesh_renderer_material = [this](entt::entity entity_handle) -> std::string
        {
            return describeMeshRendererMaterial(entity_handle);
        };
        actions.describe_asset = [this](AssetID asset_id) -> std::string
        {
            if (asset_id.value == 0)
                return "None";
            auto registry = m_services.resources ? m_services.resources->getRegistry() : nullptr;
            const AssetMetadata* meta = registry ? registry->find(asset_id) : nullptr;
            return displayNameForAssetMeta(meta);
        };
        return actions;
    }

    EditorCommandActions EditorLayer::buildCommandActions()
    {
        EditorCommandActions actions{};
        actions.submit_editor_command = [this](std::unique_ptr<IEditorCommand> command)
        {
            if (!command)
                return;
            if (m_mode_callbacks.is_play_mode && m_mode_callbacks.is_play_mode())
                return;
            m_command_history.execute(std::move(command), m_editor_ui.context());
        };
        actions.undo = [this]() -> bool
        {
            if (m_mode_callbacks.is_play_mode && m_mode_callbacks.is_play_mode())
                return false;
            return m_command_history.undo(m_editor_ui.context());
        };
        actions.redo = [this]() -> bool
        {
            if (m_mode_callbacks.is_play_mode && m_mode_callbacks.is_play_mode())
                return false;
            return m_command_history.redo(m_editor_ui.context());
        };
        actions.can_undo = [this]() -> bool
        {
            if (m_mode_callbacks.is_play_mode && m_mode_callbacks.is_play_mode())
                return false;
            return m_command_history.canUndo();
        };
        actions.can_redo = [this]() -> bool
        {
            if (m_mode_callbacks.is_play_mode && m_mode_callbacks.is_play_mode())
                return false;
            return m_command_history.canRedo();
        };
        actions.execute_command = [this](EditorCommandId id) -> bool
        {
            return executeCommand(id);
        };
        actions.can_execute_command = [this](EditorCommandId id) -> bool
        {
            return canExecuteCommand(id);
        };
        actions.commit_transform_command = [this](entt::entity entity,
                                                  const TransformSnapshot& before,
                                                  const TransformSnapshot& after)
        {
            auto& ctx = m_editor_ui.context();
            if (entity == entt::null || !ctx.document.activeDocument())
                return;
            if (ctx.mode.is_play_mode && ctx.mode.is_play_mode())
                return;
            if (TransformSnapshotsEqual(before, after))
                return;

            auto command = std::make_unique<TransformCommand>(ctx.document.activeDocument(), entity, before, after);
            m_command_history.execute(std::move(command), ctx);
        };
        return actions;
    }

    EditorModeActions EditorLayer::buildModeActions()
    {
        EditorModeActions actions{};
        actions.enter_play_mode = [this]()
        {
            const auto& document = m_scene_io.getActiveDocument();
            if (!document || !document->scene)
                return;

            if (m_mode_callbacks.enter_play_mode_from_scene)
            {
                if (m_mode_callbacks.enter_play_mode_from_scene(document->scene))
                {
                    auto& local_ctx = m_editor_ui.context();
                    local_ctx.selection.clear();
                    local_ctx.picking.request = false;
                    syncContextDocumentState();
                }
            }
        };
        actions.exit_play_mode = [this]()
        {
            if (m_mode_callbacks.exit_play_mode)
            {
                m_mode_callbacks.exit_play_mode();
                auto& local_ctx = m_editor_ui.context();
                local_ctx.selection.clear();
                local_ctx.picking.request = false;
                syncContextDocumentState();
            }
        };
        actions.toggle_pause_mode = [this]()
        {
            if (m_mode_callbacks.toggle_pause_mode)
                m_mode_callbacks.toggle_pause_mode();
        };
        actions.is_play_mode = [this]() -> bool
        {
            return m_mode_callbacks.is_play_mode ? m_mode_callbacks.is_play_mode() : false;
        };
        actions.is_pause_mode = [this]() -> bool
        {
            return m_mode_callbacks.is_pause_mode ? m_mode_callbacks.is_pause_mode() : false;
        };
        return actions;
    }

    EditorContextActionBindings EditorLayer::buildContextActionBindings()
    {
        EditorContextActionBindings bindings{};
        bindings.documents = buildDocumentActions();
        bindings.scene_actions = buildSceneActions();
        bindings.asset_actions = buildAssetActions();
        bindings.commands = buildCommandActions();
        bindings.mode = buildModeActions();
        return bindings;
    }

    bool EditorLayer::requestOpenProject()
    {
        auto& ctx = m_editor_ui.context();
        if (!m_services.platform)
        {
            ctx.setStatusMessage("Platform file dialog service is unavailable.");
            return false;
        }

        OpenFileDialogDesc dialog_desc{};
        dialog_desc.title = "Open Project";
        dialog_desc.filters = {{"Hybrid Project", "*.hyproj"}};
        const ProjectContext& project = ProjectService::Get();
        dialog_desc.initial_dir = project.project_file.empty() ? project.root : project.project_file.parent_path();

        const auto selected_paths = m_services.platform->showOpenFileDialog(m_services.window ? m_services.window->getNativeWindow() : nullptr,
                                                                            dialog_desc);
        if (selected_paths.empty())
            return false;

        return openProjectInNewInstance(selected_paths.front());
    }

    bool EditorLayer::openProjectInNewInstance(const std::filesystem::path& requested_project_path)
    {
        auto& ctx = m_editor_ui.context();
        const ProjectContext& project = ProjectService::Get();
        if (!m_services.platform)
        {
            ctx.setStatusMessage("Platform services are unavailable.");
            return false;
        }

        std::filesystem::path resolved_project_file;
        std::string resolve_error;
        if (!ResolveProjectFilePath(requested_project_path, resolved_project_file, resolve_error))
        {
            ctx.setStatusMessage("Failed to resolve selected project.");
            return false;
        }

        std::error_code ec;
        const std::filesystem::path current_project = std::filesystem::weakly_canonical(project.project_file, ec);
        ec.clear();
        const std::filesystem::path target_project = std::filesystem::weakly_canonical(resolved_project_file, ec);
        if (!current_project.empty() && !target_project.empty() && current_project == target_project)
        {
            // TODO: Replace this status-message placeholder with a unified editor message box once available.
            ctx.setStatusMessage("Project already open.");
            return false;
        }

        ProjectInstanceLock target_lock;
        std::string lock_error;
        if (!target_lock.acquire(target_project, lock_error))
        {
            ctx.setStatusMessage("Project is already open in another editor instance.");
            return false;
        }
        target_lock.release();

        const std::filesystem::path editor_executable = m_services.platform->getCurrentExecutablePath();
        if (editor_executable.empty())
        {
            ctx.setStatusMessage("Failed to resolve the editor executable path.");
            return false;
        }

        if (!m_services.platform->launchEditorProcess(editor_executable,
                                                      {"--project", target_project.string()}))
        {
            ctx.setStatusMessage("Failed to launch a new editor instance.");
            return false;
        }

        ctx.setStatusMessage("Opened project in a new editor instance.");
        return true;
    }

    void EditorLayer::syncContextDocumentState()
    {
        syncSceneViewState();

        auto& ctx = m_editor_ui.context();
        ctx.setActiveDocument(m_scene_io.getActiveDocument());
        if (m_mode_callbacks.is_play_mode && m_mode_callbacks.is_play_mode() &&
            m_services.scene && m_services.scene->hasActiveScene())
        {
            ctx.document.setSceneOverride(m_services.scene->getActiveScene().get());
        }
        else
        {
            ctx.document.clearSceneOverride();
        }

        if (ctx.document.activeScene() && ctx.activeEntity() != entt::null)
        {
            auto& reg = ctx.document.activeScene()->getRegistry();
            for (auto it = ctx.selection.items().begin(); it != ctx.selection.items().end();)
            {
                if (!reg.valid(*it))
                    it = ctx.selection.items().erase(it);
                else
                    ++it;
            }

            if (ctx.selection.active() != entt::null && !reg.valid(ctx.selection.active()))
                ctx.selection.setActive(entt::null);
            if (ctx.selection.rangeAnchor() != entt::null && !reg.valid(ctx.selection.rangeAnchor()))
                ctx.selection.setRangeAnchor(entt::null);
            if (ctx.selection.items().empty())
                ctx.selection.clear();
        }

        ctx.setStatusMessage(m_scene_io.getStatusMessage());
        m_editor_ui.setActiveScene(ctx.document.activeScene());
    }

    void EditorLayer::syncSceneViewState()
    {
        const auto& document = m_scene_io.getActiveDocument();
        if (document == m_active_scene_view_document)
            return;

        auto& ctx = m_editor_ui.context();
        ctx.selection.clear();
        ctx.picking.request = false;
        m_command_history.clear();

        if (m_active_scene_view_document)
            (void)m_scene_io.saveSceneViewState(*m_active_scene_view_document, m_editor_camera.dumpState());

        if (document && document->isSaved())
        {
            EditorCameraState state = m_editor_camera.dumpState();
            if (m_scene_io.loadSceneViewState(*document, state))
                m_editor_camera.loadState(state);
        }

        m_active_scene_view_document = document;
    }

    void EditorLayer::onUpdate(float dt)
    {
        if (!m_initialized)
            return;

        syncContextDocumentState();
        m_editor_ui.updateViewportState();
        updateEditorCamera(dt);
        m_asset_hot_reload_controller.update(dt);

        if (m_services.consume_pick_result)
        {
            uint32_t picked = 0;
            if (m_services.consume_pick_result(picked))
            {
                auto& ctx = m_editor_ui.context();
                if (picked == kInvalidEntityID || !ctx.document.activeScene())
                {
                    if (!ctx.picking.toggle)
                        ctx.selection.clear();
                }
                else
                {
                    const entt::entity entity = static_cast<entt::entity>(picked);
                    auto& reg = ctx.document.activeScene()->getRegistry();
                    const entt::entity resolved = reg.valid(entity) ? entity : entt::null;
                    if (!ctx.picking.toggle)
                    {
                        ctx.selection.setSingle(resolved);
                    }
                    else if (resolved != entt::null)
                    {
                        ctx.selection.toggle(resolved);
                    }
                }
                ctx.picking.toggle = false;
            }
        }

        updateFrameContext();
    }

    void EditorLayer::onImGuiRender()
    {
        if (!m_initialized)
            return;

        syncContextDocumentState();

        auto& ctx = m_editor_ui.context();
        ctx.gizmo.view = m_services.render->getLastView();
        ctx.gizmo.proj = m_services.render->getLastProj();

        m_editor_ui.drawPanels();
        m_editor_ui.drawViewports(m_services.render->getSceneColorTexture(),
                                  m_services.render->getGameColorTexture());
    }

    void EditorLayer::updateFrameContext()
    {
        auto* frame_context = m_services.frame_context;
        auto* render_flags = m_services.render_flags;
        auto* editor_ext = m_services.editor_ext;
        if (!frame_context || !render_flags || !editor_ext)
            return;

        auto& ctx = m_editor_ui.context();
        frame_context->viewport_size = {ctx.scene_viewport.size.x, ctx.scene_viewport.size.y};
        editor_ext->viewport_active = ctx.scene_viewport.image_hovered;
        editor_ext->render_scene_view = ctx.scene_viewport.size.x > 1.0f && ctx.scene_viewport.size.y > 1.0f;
        editor_ext->render_game_view = ctx.game_viewport.size.x > 1.0f && ctx.game_viewport.size.y > 1.0f;
        editor_ext->scene_viewport_size = {ctx.scene_viewport.size.x, ctx.scene_viewport.size.y};
        editor_ext->game_viewport_size = {ctx.game_viewport.size.x, ctx.game_viewport.size.y};
        editor_ext->selection.selected_entities.clear();
        editor_ext->selection.selected_entities.reserve(ctx.selection.items().size());
        for (entt::entity selected : ctx.selection.items())
        {
            if (selected == entt::null)
                continue;
            editor_ext->selection.selected_entities.push_back(static_cast<uint32_t>(entt::to_integral(selected)));
        }
        editor_ext->selection.active_entity =
            (ctx.activeEntity() == entt::null) ? kInvalidEntityID : static_cast<uint32_t>(entt::to_integral(ctx.activeEntity()));
        editor_ext->selection.hovered_entity = kInvalidEntityID;
        editor_ext->select_tool = ctx.gizmo.select_tool;
        editor_ext->show_collider_debug = ctx.debug.show_collider_debug;
        editor_ext->show_shadow_debug = ctx.debug.show_shadow_debug;

        if (editor_ext->render_scene_view)
        {
            editor_ext->has_editor_camera = true;
            editor_ext->editor_view = m_editor_camera.getView();
            editor_ext->editor_proj = m_editor_camera.getProjection();
            editor_ext->editor_camera_pos = m_editor_camera.getPosition();
        }
        else
        {
            editor_ext->has_editor_camera = false;
        }

        *render_flags = RenderFlags::Scene | RenderFlags::PickingID | RenderFlags::Grid | RenderFlags::Gizmo | RenderFlags::Shadow;
        if (!editor_ext->selection.selected_entities.empty())
            *render_flags |= RenderFlags::SelectionHighlight;

        if (ctx.picking.request)
        {
            editor_ext->request_pick = true;
            editor_ext->pick_x = ctx.picking.x;
            editor_ext->pick_y = ctx.picking.y;
            ctx.picking.request = false;
        }
        else
        {
            editor_ext->request_pick = false;
        }
    }

    AssetID EditorLayer::findAssetByVPath(const std::string& asset_vpath) const
    {
        if (!m_services.resources)
            return {};

        auto registry = m_services.resources->getRegistry();
        if (!registry)
            return {};

        if (const auto* meta = registry->findByPath(asset_vpath))
            return meta->id;

        return {};
    }

    std::string EditorLayer::describeMeshRendererMaterial(entt::entity entity_handle) const
    {
        std::shared_ptr<Scene> scene;
        if (m_mode_callbacks.is_play_mode && m_mode_callbacks.is_play_mode() &&
            m_services.scene && m_services.scene->hasActiveScene())
        {
            scene = m_services.scene->getActiveScene();
        }
        else if (m_scene_io.getActiveDocument())
        {
            scene = m_scene_io.getActiveDocument()->scene;
        }

        if (!scene || entity_handle == entt::null)
            return "None";

        auto& registry = scene->getRegistry();
        if (!registry.valid(entity_handle) || !registry.all_of<MeshRendererComponent>(entity_handle))
            return "None";

        const auto& mr = registry.get<MeshRendererComponent>(entity_handle);
        auto registry_ptr = m_services.resources ? m_services.resources->getRegistry() : nullptr;
        auto manager = m_services.resources ? m_services.resources->getManager() : nullptr;

        if (mr.Material.value != 0)
        {
            const AssetMetadata* meta = registry_ptr ? registry_ptr->find(mr.Material) : nullptr;
            return displayNameForAssetMeta(meta);
        }

        if (mr.Mesh.value != 0 && manager)
        {
            if (auto mesh = manager->loadSync<Mesh>(mr.Mesh))
            {
                AssetID first_material{};
                bool multiple_materials = false;
                for (const auto& submesh : mesh->getSubmeshes())
                {
                    if (submesh.material.value == 0)
                        continue;

                    if (first_material.value == 0)
                    {
                        first_material = submesh.material;
                        continue;
                    }

                    if (first_material != submesh.material)
                    {
                        multiple_materials = true;
                        break;
                    }
                }

                if (multiple_materials)
                    return "Multiple submesh materials";

                if (first_material.value != 0)
                {
                    const AssetMetadata* meta = registry_ptr ? registry_ptr->find(first_material) : nullptr;
                    return displayNameForAssetMeta(meta) + " (Mesh)";
                }
            }
        }

        return "HybridDefault";
    }

    entt::entity EditorLayer::createSceneEntity(SceneEntityTemplate type, entt::entity parent)
    {
        auto& ctx = m_editor_ui.context();
        if (!ctx.document.activeScene() || !ctx.document.activeDocument())
            return entt::null;
        if (ctx.mode.is_play_mode && ctx.mode.is_play_mode())
            return entt::null;

        Scene& scene = *ctx.document.activeScene();
        auto& registry = scene.getRegistry();
        const bool valid_parent = parent != entt::null && registry.valid(parent) && registry.all_of<TransformComponent>(parent);
        const EditorSelectionSnapshot before_selection = CaptureSelectionSnapshot(ctx, &scene);

        Entity created{};
        switch (type)
        {
        case SceneEntityTemplate::Empty:
            created = scene.createEntity("Empty");
            break;
        case SceneEntityTemplate::Cube:
        {
            const AssetID cube_mesh_id = m_services.resources ? m_services.resources->getBuiltinMeshID(BuiltinMesh::Cube) : AssetID{};
            if (cube_mesh_id.value == 0)
                return entt::null;
            created = scene.createEntity("Cube");
            auto& renderer = created.AddComponent<MeshRendererComponent>();
            renderer.Mesh = cube_mesh_id;
            break;
        }
        case SceneEntityTemplate::Camera:
            created = scene.createCameraEntity("Camera", false);
            break;
        case SceneEntityTemplate::DirectionalLight:
            created = scene.createEntity("Directional Light");
            created.AddComponent<DirectionalLightComponent>();
            break;
        case SceneEntityTemplate::PointLight:
            created = scene.createEntity("Point Light");
            created.AddComponent<PointLightComponent>();
            break;
        }

        if (!created)
            return entt::null;

        if (valid_parent)
            scene.SetParent(created, Entity(parent, &registry, &scene), true);

        const EntitySnapshot snapshot = CaptureEntitySubtree(scene, created.GetHandle());

        auto command = std::make_unique<CreateEntityCommand>(ctx.document.activeDocument(),
                                                             snapshot,
                                                             valid_parent ? parent : entt::null,
                                                             before_selection);
        m_command_history.execute(std::move(command), ctx);
        syncContextDocumentState();
        return ctx.activeEntity();
    }

    bool EditorLayer::deleteSceneEntity(entt::entity entity_handle)
    {
        auto& ctx = m_editor_ui.context();
        if (!ctx.document.activeScene() || !ctx.document.activeDocument() || entity_handle == entt::null)
            return false;
        if (ctx.mode.is_play_mode && ctx.mode.is_play_mode())
            return false;

        Scene& scene = *ctx.document.activeScene();
        auto& registry = scene.getRegistry();
        if (!registry.valid(entity_handle) || !registry.all_of<TransformComponent>(entity_handle))
            return false;

        const EditorSelectionSnapshot before_selection = CaptureSelectionSnapshot(ctx, &scene);
        const EntitySnapshot snapshot = CaptureEntitySubtree(scene, entity_handle);
        const entt::entity parent =
            registry.get<TransformComponent>(entity_handle).Parent;

        EditorSelectionModel after_selection = ctx.selection.state();
        std::vector<entt::entity> removed_entities;
        std::function<void(entt::entity)> collect_subtree = [&](entt::entity current)
        {
            if (current == entt::null || !registry.valid(current) || !registry.all_of<TransformComponent>(current))
                return;
            removed_entities.push_back(current);
            const auto& transform = registry.get<TransformComponent>(current);
            for (entt::entity child = transform.FirstChild; child != entt::null;)
            {
                if (!registry.valid(child) || !registry.all_of<TransformComponent>(child))
                    break;
                const entt::entity next = registry.get<TransformComponent>(child).NextSibling;
                collect_subtree(child);
                child = next;
            }
        };
        collect_subtree(entity_handle);
        for (entt::entity removed : removed_entities)
            after_selection.remove(removed);

        const EditorSelectionSnapshot after_selection_snapshot = [&]()
        {
            EditorSelectionSnapshot snapshot_result{};
            snapshot_result.items.reserve(after_selection.items.size());
            for (entt::entity selected : after_selection.items)
            {
                if (!registry.valid(selected) || !registry.all_of<IDComponent>(selected))
                    continue;
                snapshot_result.items.push_back(registry.get<IDComponent>(selected).ID);
            }
            if (after_selection.active != entt::null &&
                registry.valid(after_selection.active) &&
                registry.all_of<IDComponent>(after_selection.active))
            {
                snapshot_result.active = registry.get<IDComponent>(after_selection.active).ID;
            }
            if (after_selection.range_anchor != entt::null &&
                registry.valid(after_selection.range_anchor) &&
                registry.all_of<IDComponent>(after_selection.range_anchor))
            {
                snapshot_result.range_anchor = registry.get<IDComponent>(after_selection.range_anchor).ID;
            }
            return snapshot_result;
        }();

        auto command = std::make_unique<DeleteEntityCommand>(ctx.document.activeDocument(),
                                                             snapshot,
                                                             parent,
                                                             before_selection,
                                                             after_selection_snapshot);
        m_command_history.execute(std::move(command), ctx);
        syncContextDocumentState();
        return true;
    }

    bool EditorLayer::duplicateSceneSelection(entt::entity target_entity)
    {
        auto& ctx = m_editor_ui.context();
        if (!ctx.document.activeScene() || !ctx.document.activeDocument())
            return false;
        if (ctx.mode.is_play_mode && ctx.mode.is_play_mode())
            return false;

        Scene& scene = *ctx.document.activeScene();
        auto& registry = scene.getRegistry();
        if (target_entity == entt::null || !registry.valid(target_entity) || !registry.all_of<TransformComponent>(target_entity))
            return false;

        std::vector<entt::entity> source_entities;
        if (ctx.selection.contains(target_entity))
        {
            source_entities = ctx.selection.items();
        }
        else
        {
            source_entities.push_back(target_entity);
        }

        source_entities.erase(
            std::remove_if(source_entities.begin(),
                           source_entities.end(),
                           [&](entt::entity entity)
                           {
                               return entity == entt::null || !registry.valid(entity) || !registry.all_of<TransformComponent>(entity);
                           }),
            source_entities.end());

        std::vector<entt::entity> root_entities;
        root_entities.reserve(source_entities.size());
        for (entt::entity entity : source_entities)
        {
            bool is_child_of_selected = false;
            for (entt::entity other : source_entities)
            {
                if (other == entity)
                    continue;
                if (scene.IsDescendant(Entity(entity, &registry, &scene), Entity(other, &registry, &scene)))
                {
                    is_child_of_selected = true;
                    break;
                }
            }

            if (!is_child_of_selected)
                root_entities.push_back(entity);
        }

        if (root_entities.empty())
            return false;

        const EditorSelectionSnapshot before_selection = CaptureSelectionSnapshot(ctx, &scene);
        std::vector<DuplicateEntityCommand::Entry> entries;
        entries.reserve(root_entities.size());

        UUID new_active_id{};
        for (entt::entity entity : root_entities)
        {
            DuplicateEntityCommand::Entry entry{};
            entry.snapshot = CloneEntitySnapshotWithFreshUUIDs(CaptureEntitySubtree(scene, entity));
            entry.parent_id = {};

            const entt::entity parent = registry.get<TransformComponent>(entity).Parent;
            if (parent != entt::null && registry.valid(parent) && registry.all_of<IDComponent>(parent))
                entry.parent_id = registry.get<IDComponent>(parent).ID;

            if (entity == ctx.selection.active())
                new_active_id = entry.snapshot.id;

            entries.push_back(std::move(entry));
        }

        EditorSelectionSnapshot after_selection{};
        after_selection.items.reserve(entries.size());
        for (const auto& entry : entries)
            after_selection.items.push_back(entry.snapshot.id);

        if (new_active_id.value != 0)
            after_selection.active = new_active_id;
        else if (!entries.empty())
            after_selection.active = entries.back().snapshot.id;

        after_selection.range_anchor = after_selection.active;

        auto command = std::make_unique<DuplicateEntityCommand>(ctx.document.activeDocument(),
                                                                std::move(entries),
                                                                before_selection,
                                                                after_selection);
        m_command_history.execute(std::move(command), ctx);
        syncContextDocumentState();
        return true;
    }

    bool EditorLayer::instantiateSceneProjectPath(const std::string& rel_path, const ImVec2& drop_mouse_pos)
    {
        if (rel_path.empty())
        {
            HBD_CORE_WARN("{} instantiate_project_path_rejected reason=empty_path", kEditorLayerLogTag);
            return false;
        }

        const std::string vpath = std::string("asset:") + std::filesystem::path(rel_path).generic_string();
        AssetID asset_id = findAssetByVPath(vpath);

        if (asset_id.value == 0)
        {
            if (!m_services.editor_resources)
            {
                m_editor_ui.context().setStatusMessage("Cannot import dropped asset.");
                HBD_CORE_WARN("{} instantiate_project_path_failed path={} reason=editor_resources_unavailable",
                              kEditorLayerLogTag,
                              vpath);
                return false;
            }

            ImportRequest request{};
            request.source_path = vpath;
            const ImportResult result = m_services.editor_resources->importAsset(request);
            if (!result.success || result.primary_id.value == 0)
            {
                m_editor_ui.context().setStatusMessage(
                    result.message.empty() ? "Asset import failed." : result.message);
                HBD_CORE_WARN("{} instantiate_project_path_failed path={} reason=import_failed message={}",
                              kEditorLayerLogTag,
                              vpath,
                              result.message.empty() ? "<empty>" : result.message);
                return false;
            }

            asset_id = result.primary_id;
        }

        const bool instantiated = instantiateSceneAsset(asset_id, drop_mouse_pos);
        if (instantiated)
        {
            HBD_CORE_INFO("{} instantiate_project_path_completed path={} asset_id={}",
                          kEditorLayerLogTag,
                          vpath,
                          asset_id.value);
        }
        return instantiated;
    }

    bool EditorLayer::tryGetSceneDropPosition(const ImVec2& drop_mouse_pos, glm::vec3& out_position)
    {
        const auto& ctx = m_editor_ui.context();
        const float viewport_width = ctx.scene_viewport.size.x;
        const float viewport_height = ctx.scene_viewport.size.y;
        if (viewport_width <= 1.0f || viewport_height <= 1.0f)
            return false;

        const float local_x = drop_mouse_pos.x - ctx.scene_viewport.min.x;
        const float local_y = drop_mouse_pos.y - ctx.scene_viewport.min.y;
        if (local_x < 0.0f || local_y < 0.0f || local_x > viewport_width || local_y > viewport_height)
            return false;

        const float ndc_x = (2.0f * local_x / viewport_width) - 1.0f;
        const float ndc_y = 1.0f - (2.0f * local_y / viewport_height);

        const glm::mat4 inv_view_proj = glm::inverse(m_editor_camera.getProjection() * m_editor_camera.getView());
        const Ray ray = MakeRayFromInvViewProjection(inv_view_proj, ndc_x, ndc_y);
        return IntersectPlaneY0(ray, out_position);
    }

    bool EditorLayer::instantiateSceneAsset(AssetID asset_id, const ImVec2& drop_mouse_pos)
    {
        auto& ctx = m_editor_ui.context();
        if (asset_id.value == 0 || !ctx.document.activeScene())
        {
            HBD_CORE_WARN("{} instantiate_asset_rejected asset_id={} active_scene_ready={}",
                          kEditorLayerLogTag,
                          asset_id.value,
                          ctx.document.activeScene() != nullptr);
            return false;
        }

        if (ctx.mode.is_play_mode && ctx.mode.is_play_mode())
        {
            ctx.setStatusMessage("Cannot instantiate assets during Play Mode.");
            HBD_CORE_WARN("{} instantiate_asset_rejected asset_id={} reason=play_mode",
                          kEditorLayerLogTag,
                          asset_id.value);
            return false;
        }

        if (!m_services.resources)
        {
            HBD_CORE_WARN("{} instantiate_asset_failed asset_id={} reason=resource_system_unavailable",
                          kEditorLayerLogTag,
                          asset_id.value);
            return false;
        }

        auto registry = m_services.resources->getRegistry();
        if (!registry)
        {
            HBD_CORE_WARN("{} instantiate_asset_failed asset_id={} reason=asset_registry_unavailable",
                          kEditorLayerLogTag,
                          asset_id.value);
            return false;
        }

        const auto* meta = registry->find(asset_id);
        if (!meta || !meta->is_valid)
        {
            ctx.setStatusMessage("Dropped asset is invalid.");
            HBD_CORE_WARN("{} instantiate_asset_failed asset_id={} reason=invalid_asset_metadata",
                          kEditorLayerLogTag,
                          asset_id.value);
            return false;
        }

        if (meta->type != AssetType::Mesh)
        {
            if (meta->type == AssetType::Scene)
                ctx.setStatusMessage("Scene assets cannot be instantiated into Scene View.");
            else
                ctx.setStatusMessage("Dropped asset type is not instantiable yet.");
            HBD_CORE_WARN("{} instantiate_asset_failed asset_id={} asset_type={} reason=unsupported_asset_type",
                          kEditorLayerLogTag,
                          asset_id.value,
                          static_cast<uint32_t>(meta->type));
            return false;
        }

        std::string name = "Mesh";
        if (!meta->source_path.empty())
            name = std::filesystem::path(meta->source_path.substr(meta->source_path.find(':') + 1)).stem().string();
        if (name.empty())
            name = "Mesh";

        glm::vec3 drop_position{};
        if (!tryGetSceneDropPosition(drop_mouse_pos, drop_position))
        {
            ctx.setStatusMessage("Cannot resolve drop position on ground plane.");
            HBD_CORE_WARN("{} instantiate_asset_failed asset_id={} reason=drop_position_unresolved x={} y={}",
                          kEditorLayerLogTag,
                          asset_id.value,
                          drop_mouse_pos.x,
                          drop_mouse_pos.y);
            return false;
        }

        Entity entity = ctx.document.activeScene()->createRenderableEntity(name);
        auto& tr = entity.GetComponent<TransformComponent>();
        tr.Position = drop_position;
        tr.DirtyLocal = true;
        tr.DirtyWorld = true;

        auto& renderer = entity.GetComponent<MeshRendererComponent>();
        renderer.Mesh = asset_id;

        ctx.document.activeScene()->MarkDirtyRecursive(entity);
        ctx.selection.setSingle(entity.GetHandle());
        ctx.markSceneDirty();
        ctx.setStatusMessage("Instantiated mesh into scene.");
        HBD_CORE_INFO("{} instantiate_asset_completed asset_id={} entity={} x={} y={} z={}",
                      kEditorLayerLogTag,
                      asset_id.value,
                      entt::to_integral(entity.GetHandle()),
                      drop_position.x,
                      drop_position.y,
                      drop_position.z);
        return true;
    }

    bool EditorLayer::fitBoxColliderToMesh(entt::entity entity_handle)
    {
        auto& ctx = m_editor_ui.context();
        if (!ctx.document.activeScene() || entity_handle == entt::null)
        {
            HBD_CORE_WARN("{} fit_box_collider_rejected entity={} active_scene_ready={}",
                          kEditorLayerLogTag,
                          entt::to_integral(entity_handle),
                          ctx.document.activeScene() != nullptr);
            return false;
        }

        auto& registry = ctx.document.activeScene()->getRegistry();
        if (!registry.valid(entity_handle))
        {
            HBD_CORE_WARN("{} fit_box_collider_rejected entity={} reason=invalid_entity",
                          kEditorLayerLogTag,
                          entt::to_integral(entity_handle));
            return false;
        }

        Entity entity(entity_handle, &registry, ctx.document.activeScene());
        if (!entity.HasComponent<ColliderComponent>() || !entity.HasComponent<MeshRendererComponent>())
        {
            HBD_CORE_WARN("{} fit_box_collider_rejected entity={} reason=missing_required_components",
                          kEditorLayerLogTag,
                          entt::to_integral(entity_handle));
            return false;
        }
        if (!m_services.resources)
        {
            HBD_CORE_WARN("{} fit_box_collider_failed entity={} reason=resource_system_unavailable",
                          kEditorLayerLogTag,
                          entt::to_integral(entity_handle));
            return false;
        }

        const auto& mesh_renderer = entity.GetComponent<MeshRendererComponent>();
        if (mesh_renderer.Mesh.value == 0)
        {
            ctx.setStatusMessage("Fit To Mesh requires a valid mesh.");
            HBD_CORE_WARN("{} fit_box_collider_failed entity={} reason=missing_mesh",
                          kEditorLayerLogTag,
                          entt::to_integral(entity_handle));
            return false;
        }

        auto manager = m_services.resources->getManager();
        if (!manager)
        {
            HBD_CORE_WARN("{} fit_box_collider_failed entity={} reason=asset_manager_unavailable",
                          kEditorLayerLogTag,
                          entt::to_integral(entity_handle));
            return false;
        }

        auto mesh = manager->loadSync<Mesh>(mesh_renderer.Mesh);
        if (!mesh)
        {
            ctx.setStatusMessage("Failed to load mesh for collider fitting.");
            HBD_CORE_WARN("{} fit_box_collider_failed entity={} mesh_id={} reason=mesh_load_failed",
                          kEditorLayerLogTag,
                          entt::to_integral(entity_handle),
                          mesh_renderer.Mesh.value);
            return false;
        }

        bool has_bounds = false;
        glm::vec3 aabb_min(0.0f);
        glm::vec3 aabb_max(0.0f);

        const auto& submeshes = mesh->getSubmeshes();
        for (const auto& submesh : submeshes)
        {
            if (!has_bounds)
            {
                aabb_min = submesh.aabb_min;
                aabb_max = submesh.aabb_max;
                has_bounds = true;
            }
            else
            {
                    aabb_min = (glm::min)(aabb_min, submesh.aabb_min);
                    aabb_max = (glm::max)(aabb_max, submesh.aabb_max);
            }
        }

        if (!has_bounds)
        {
            const auto& vertices = mesh->getVertices();
            for (const auto& vertex : vertices)
            {
                if (!has_bounds)
                {
                    aabb_min = vertex.position;
                    aabb_max = vertex.position;
                    has_bounds = true;
                }
                else
                {
                    aabb_min = (glm::min)(aabb_min, vertex.position);
                    aabb_max = (glm::max)(aabb_max, vertex.position);
                }
            }
        }

        if (!has_bounds)
        {
            ctx.setStatusMessage("Mesh has no bounds to fit collider.");
            HBD_CORE_WARN("{} fit_box_collider_failed entity={} mesh_id={} reason=missing_bounds",
                          kEditorLayerLogTag,
                          entt::to_integral(entity_handle),
                          mesh_renderer.Mesh.value);
            return false;
        }

        auto& collider = entity.GetComponent<ColliderComponent>();
        collider.Type = ColliderType::Box;
        collider.Center = (aabb_min + aabb_max) * 0.5f;
        collider.Box.HalfExtents = (glm::max)((aabb_max - aabb_min) * 0.5f, glm::vec3(0.0f));

        ctx.document.activeScene()->MarkDirtyRecursive(entity);
        ctx.markSceneDirty();
        ctx.setStatusMessage("Box collider fitted to mesh.");
        HBD_CORE_INFO("{} fit_box_collider_completed entity={} mesh_id={}",
                      kEditorLayerLogTag,
                      entt::to_integral(entity_handle),
                      mesh_renderer.Mesh.value);
        return true;
    }

    void EditorLayer::updateEditorCamera(float dt)
    {
        auto& ctx = m_editor_ui.context();
        ctx.gizmo.suppress_tool_shortcuts = false;

        if (!m_services.input || !m_services.editor_ext)
            return;

        if (ctx.scene_viewport.size.x > 1.0f && ctx.scene_viewport.size.y > 1.0f)
            m_editor_camera.setViewportSize(ctx.scene_viewport.size.x, ctx.scene_viewport.size.y);

        const bool camera_input_active = ctx.scene_viewport.image_hovered;
        const InputState& input = m_services.input->getState();

        const float mdx = camera_input_active ? input.getMouseDeltaX() : 0.0f;
        const float mdy = camera_input_active ? input.getMouseDeltaY() : 0.0f;
        const float scroll_y = camera_input_active ? input.getScrollDeltaY() : 0.0f;

        const bool lmb_down = camera_input_active && input.isMouseDown(GLFW_MOUSE_BUTTON_LEFT);
        const bool mmb_down = camera_input_active && input.isMouseDown(GLFW_MOUSE_BUTTON_MIDDLE);
        const bool rmb_down = camera_input_active && input.isMouseDown(GLFW_MOUSE_BUTTON_RIGHT);
        const bool mmb_for_camera = mmb_down || (ctx.gizmo.select_tool && lmb_down);

        const bool key_w = camera_input_active && input.isKeyDown(GLFW_KEY_W);
        const bool key_a = camera_input_active && input.isKeyDown(GLFW_KEY_A);
        const bool key_s = camera_input_active && input.isKeyDown(GLFW_KEY_S);
        const bool key_d = camera_input_active && input.isKeyDown(GLFW_KEY_D);
        const bool key_q = camera_input_active && input.isKeyDown(GLFW_KEY_Q);
        const bool key_e = camera_input_active && input.isKeyDown(GLFW_KEY_E);
        const bool key_shift = camera_input_active &&
            (input.isKeyDown(GLFW_KEY_LEFT_SHIFT) || input.isKeyDown(GLFW_KEY_RIGHT_SHIFT));
        const bool key_ctrl = camera_input_active &&
            (input.isKeyDown(GLFW_KEY_LEFT_CONTROL) || input.isKeyDown(GLFW_KEY_RIGHT_CONTROL));
        const bool key_alt = camera_input_active &&
            (input.isKeyDown(GLFW_KEY_LEFT_ALT) || input.isKeyDown(GLFW_KEY_RIGHT_ALT));

        ctx.gizmo.suppress_tool_shortcuts = camera_input_active && rmb_down && !key_alt;

        m_editor_camera.update(dt,
                               camera_input_active,
                               mdx,
                               mdy,
                               scroll_y,
                               lmb_down,
                               mmb_for_camera,
                               rmb_down,
                               key_w,
                               key_a,
                               key_s,
                               key_d,
                               key_q,
                               key_e,
                               key_shift,
                               key_ctrl,
                               key_alt);
    }

    void EditorLayer::setModeCallbacks(EditorModeCallbacks callbacks)
    {
        m_mode_callbacks = std::move(callbacks);
    }
} // namespace Hybrid
