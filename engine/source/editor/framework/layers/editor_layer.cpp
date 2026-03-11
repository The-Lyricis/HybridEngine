#include "editor_layer.h"

#include <cmath>
#include <entt/entity/entity.hpp>
#include <glm/glm.hpp>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "editor/core/editor_context.h"
#include "editor/services/import/import_types.h"
#include "runtime/core/base/intersection.h"
#include "runtime/core/base/macro.h"
#include "runtime/modules/asset/asset_registry.h"
#include "runtime/modules/asset/asset_type.h"
#include "runtime/modules/asset/mesh.h"
#include "runtime/modules/asset/runtime_resource_system.h"
#include "runtime/modules/input/input_layer.h"
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
            HBD_CORE_ERROR("EditorLayer attach failed: missing required engine services");
            return;
        }

        m_editor_ui.initialize(m_services.window->getNativeWindow());
        m_asset_hot_reload_controller.initialize([this](const std::string& message) {
            m_editor_ui.context().setStatusMessage(message);
        });
        m_scene_io.initialize();
        m_initialized = true;

        auto& ctx = m_editor_ui.context();
        m_asset_hot_reload_controller.bindContext(ctx);
        ctx.enter_play_mode = [this]()
            {
                const auto& document = m_scene_io.getActiveDocument();
                if (!document || !document->scene)
                    return;

                if (m_mode_callbacks.enter_play_mode_from_scene)
                {
                    if (m_mode_callbacks.enter_play_mode_from_scene(document->scene))
                    {
                        auto& local_ctx = m_editor_ui.context();
                        local_ctx.selected = entt::null;
                        local_ctx.request_pick = false;
                        syncContextDocumentState();
                    }
                }
            };
        ctx.exit_play_mode = [this]()
            {
                if (m_mode_callbacks.exit_play_mode)
                {
                    m_mode_callbacks.exit_play_mode();
                    auto& local_ctx = m_editor_ui.context();
                    local_ctx.selected = entt::null;
                    local_ctx.request_pick = false;
                    syncContextDocumentState();
                }
            };
        ctx.toggle_pause_mode = [this]()
            {
                if (m_mode_callbacks.toggle_pause_mode)
                    m_mode_callbacks.toggle_pause_mode();
            };
        ctx.is_play_mode = [this]() -> bool
            {
                return m_mode_callbacks.is_play_mode ? m_mode_callbacks.is_play_mode() : false;
            };
        ctx.is_pause_mode = [this]() -> bool
            {
                return m_mode_callbacks.is_pause_mode ? m_mode_callbacks.is_pause_mode() : false;
            };
        ctx.open_scene = [this](const std::string& scene_vpath)
            {
                const bool opened = m_scene_io.open(scene_vpath);
                if (opened)
                    m_editor_ui.context().selected = entt::null;
                syncContextDocumentState();
            };
        ctx.request_reimport_asset = [this](const std::string& asset_vpath) -> bool
            {
                const bool ok = m_asset_hot_reload_controller.requestReimport(asset_vpath);
                syncContextDocumentState();
                return ok;
            };
        ctx.request_rename_folder = [this](const std::string& old_folder_vpath, const std::string& new_folder_vpath) -> bool
            {
                const bool ok = m_asset_hot_reload_controller.requestRenameFolder(old_folder_vpath, new_folder_vpath);
                syncContextDocumentState();
                return ok;
            };
        ctx.request_save_scene = [this]() -> bool
            {
                const bool saved = m_scene_io.requestSave();
                syncContextDocumentState();
                return saved;
            };
        ctx.request_save_scene_as = [this]() -> bool
            {
                const bool saved = m_scene_io.requestSaveAs();
                syncContextDocumentState();
                return saved;
            };
        ctx.find_asset_by_vpath = [this](const std::string& asset_vpath) -> AssetID
            {
                return findAssetByVPath(asset_vpath);
            };
        ctx.instantiate_scene_asset = [this](AssetID asset_id, const ImVec2& drop_mouse_pos) -> bool
            {
                const bool instantiated = instantiateSceneAsset(asset_id, drop_mouse_pos);
                syncContextDocumentState();
                return instantiated;
            };
        ctx.instantiate_scene_project_path = [this](const std::string& rel_path, const ImVec2& drop_mouse_pos) -> bool
            {
                const bool instantiated = instantiateSceneProjectPath(rel_path, drop_mouse_pos);
                syncContextDocumentState();
                return instantiated;
            };
        ctx.fit_box_collider_to_mesh = [this](entt::entity entity_handle) -> bool
            {
                const bool fitted = fitBoxColliderToMesh(entity_handle);
                syncContextDocumentState();
                return fitted;
            };
        ctx.get_builtin_mesh_id = [this](BuiltinMesh mesh) -> AssetID
            {
                if (!m_services.resources)
                    return {};
                return m_services.resources->getBuiltinMeshID(mesh);
            };

        (void)m_scene_io.restoreStartupScene();
        syncContextDocumentState();
    }

    void EditorLayer::onDetach()
    {
        if (!m_initialized)
            return;

        if (m_active_scene_view_document)
            (void)m_scene_io.saveSceneViewState(*m_active_scene_view_document, m_editor_camera.dumpState());

        auto& ctx = m_editor_ui.context();
        ctx.notify_asset_source_event = {};
        ctx.open_scene = {};
        ctx.request_reimport_asset = {};
        ctx.request_rename_folder = {};
        ctx.request_save_scene = {};
        ctx.request_save_scene_as = {};
        ctx.find_asset_by_vpath = {};
        ctx.instantiate_scene_asset = {};
        ctx.instantiate_scene_project_path = {};
        ctx.fit_box_collider_to_mesh = {};
        ctx.get_builtin_mesh_id = {};
        ctx.enter_play_mode = {};
        ctx.exit_play_mode = {};
        ctx.toggle_pause_mode = {};
        ctx.is_play_mode = {};
        ctx.is_pause_mode = {};
        ctx.selected = entt::null;
        ctx.clearActiveDocument();
        m_asset_hot_reload_controller.unbindContext(ctx);

        if (m_services.editor_ext)
            m_services.editor_ext->has_editor_camera = false;

        m_asset_hot_reload_controller.shutdown();
        m_scene_io.shutdown();
        m_editor_ui.shutdown();
        m_active_scene_view_document.reset();
        m_initialized = false;
    }

    void EditorLayer::syncContextDocumentState()
    {
        syncSceneViewState();

        auto& ctx = m_editor_ui.context();
        ctx.setActiveDocument(m_scene_io.getActiveDocument());
        if (m_mode_callbacks.is_play_mode && m_mode_callbacks.is_play_mode() &&
            m_services.scene && m_services.scene->hasActiveScene())
        {
            ctx.active_scene = m_services.scene->getActiveScene().get();
        }

        if (ctx.active_scene && ctx.selected != entt::null)
        {
            auto& reg = ctx.active_scene->getRegistry();
            if (!reg.valid(ctx.selected))
                ctx.selected = entt::null;
        }

        ctx.setStatusMessage(m_scene_io.getStatusMessage());
        m_editor_ui.setActiveScene(ctx.active_scene);
    }

    void EditorLayer::syncSceneViewState()
    {
        const auto& document = m_scene_io.getActiveDocument();
        if (document == m_active_scene_view_document)
            return;

        auto& ctx = m_editor_ui.context();
        ctx.selected = entt::null;
        ctx.request_pick = false;

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
                if (picked == kInvalidEntityID || !ctx.active_scene)
                {
                    ctx.selected = entt::null;
                }
                else
                {
                    const entt::entity entity = static_cast<entt::entity>(picked);
                    auto& reg = ctx.active_scene->getRegistry();
                    ctx.selected = reg.valid(entity) ? entity : entt::null;
                }
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
        ctx.gizmo_view = m_services.render->getLastView();
        ctx.gizmo_proj = m_services.render->getLastProj();

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
        frame_context->viewport_size = {ctx.scene_viewport_size.x, ctx.scene_viewport_size.y};
        editor_ext->viewport_active = ctx.scene_viewport_image_hovered;
        editor_ext->render_scene_view = ctx.scene_viewport_size.x > 1.0f && ctx.scene_viewport_size.y > 1.0f;
        editor_ext->render_game_view = ctx.game_viewport_size.x > 1.0f && ctx.game_viewport_size.y > 1.0f;
        editor_ext->scene_viewport_size = {ctx.scene_viewport_size.x, ctx.scene_viewport_size.y};
        editor_ext->game_viewport_size = {ctx.game_viewport_size.x, ctx.game_viewport_size.y};
        editor_ext->selected_entity_id =
            (ctx.selected == entt::null) ? kInvalidEntityID : static_cast<uint32_t>(entt::to_integral(ctx.selected));
        editor_ext->pan_tool = ctx.pan_tool;
        editor_ext->show_collider_debug = ctx.show_collider_debug;

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

        *render_flags = RenderFlags::Forward | RenderFlags::PickingID | RenderFlags::Grid | RenderFlags::Gizmos;
        if (editor_ext->selected_entity_id != kInvalidEntityID)
            *render_flags |= RenderFlags::SelectionOutline;

        if (ctx.request_pick)
        {
            editor_ext->request_pick = true;
            editor_ext->pick_x = ctx.pick_x;
            editor_ext->pick_y = ctx.pick_y;
            ctx.request_pick = false;
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

    bool EditorLayer::instantiateSceneProjectPath(const std::string& rel_path, const ImVec2& drop_mouse_pos)
    {
        if (rel_path.empty())
            return false;

        const std::string vpath = std::string("asset:") + std::filesystem::path(rel_path).generic_string();
        AssetID asset_id = findAssetByVPath(vpath);

        if (asset_id.value == 0)
        {
            if (!m_services.editor_resources)
            {
                m_editor_ui.context().setStatusMessage("Cannot import dropped asset.");
                return false;
            }

            ImportRequest request{};
            request.source_path = vpath;
            const ImportResult result = m_services.editor_resources->importAsset(request);
            if (!result.success || result.primary_id.value == 0)
            {
                m_editor_ui.context().setStatusMessage(
                    result.message.empty() ? "Asset import failed." : result.message);
                return false;
            }

            asset_id = result.primary_id;
        }

        return instantiateSceneAsset(asset_id, drop_mouse_pos);
    }

    bool EditorLayer::tryGetSceneDropPosition(const ImVec2& drop_mouse_pos, glm::vec3& out_position)
    {
        const auto& ctx = m_editor_ui.context();
        const float viewport_width = ctx.scene_viewport_size.x;
        const float viewport_height = ctx.scene_viewport_size.y;
        if (viewport_width <= 1.0f || viewport_height <= 1.0f)
            return false;

        const float local_x = drop_mouse_pos.x - ctx.scene_viewport_min.x;
        const float local_y = drop_mouse_pos.y - ctx.scene_viewport_min.y;
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
        if (asset_id.value == 0 || !ctx.active_scene)
            return false;

        if (ctx.is_play_mode && ctx.is_play_mode())
        {
            ctx.setStatusMessage("Cannot instantiate assets during Play Mode.");
            return false;
        }

        if (!m_services.resources)
            return false;

        auto registry = m_services.resources->getRegistry();
        if (!registry)
            return false;

        const auto* meta = registry->find(asset_id);
        if (!meta || !meta->is_valid)
        {
            ctx.setStatusMessage("Dropped asset is invalid.");
            return false;
        }

        if (meta->type != AssetType::Mesh)
        {
            if (meta->type == AssetType::Scene)
                ctx.setStatusMessage("Scene assets cannot be instantiated into Scene View.");
            else
                ctx.setStatusMessage("Dropped asset type is not instantiable yet.");
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
            return false;
        }

        Entity entity = ctx.active_scene->createRenderableEntity(name);
        auto& tr = entity.GetComponent<TransformComponent>();
        tr.Position = drop_position;
        tr.DirtyLocal = true;
        tr.DirtyWorld = true;

        auto& renderer = entity.GetComponent<MeshRendererComponent>();
        renderer.Mesh = asset_id;

        ctx.active_scene->MarkDirtyRecursive(entity);
        ctx.selected = entity.GetHandle();
        ctx.markSceneDirty();
        ctx.setStatusMessage("Instantiated mesh into scene.");
        return true;
    }

    bool EditorLayer::fitBoxColliderToMesh(entt::entity entity_handle)
    {
        auto& ctx = m_editor_ui.context();
        if (!ctx.active_scene || entity_handle == entt::null)
            return false;

        auto& registry = ctx.active_scene->getRegistry();
        if (!registry.valid(entity_handle))
            return false;

        Entity entity(entity_handle, &registry, ctx.active_scene);
        if (!entity.HasComponent<ColliderComponent>() || !entity.HasComponent<MeshRendererComponent>())
            return false;
        if (!m_services.resources)
            return false;

        const auto& mesh_renderer = entity.GetComponent<MeshRendererComponent>();
        if (mesh_renderer.Mesh.value == 0)
        {
            ctx.setStatusMessage("Fit To Mesh requires a valid mesh.");
            return false;
        }

        auto manager = m_services.resources->getManager();
        if (!manager)
            return false;

        auto mesh = manager->loadSync<Mesh>(mesh_renderer.Mesh);
        if (!mesh)
        {
            ctx.setStatusMessage("Failed to load mesh for collider fitting.");
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
            return false;
        }

        auto& collider = entity.GetComponent<ColliderComponent>();
        collider.Type = ColliderType::Box;
        collider.Center = (aabb_min + aabb_max) * 0.5f;
        collider.Box.HalfExtents = (glm::max)((aabb_max - aabb_min) * 0.5f, glm::vec3(0.0f));

        ctx.active_scene->MarkDirtyRecursive(entity);
        ctx.markSceneDirty();
        ctx.setStatusMessage("Box collider fitted to mesh.");
        return true;
    }

    void EditorLayer::updateEditorCamera(float dt)
    {
        auto& ctx = m_editor_ui.context();
        ctx.suppress_tool_shortcuts = false;

        if (!m_services.input || !m_services.editor_ext)
            return;

        if (ctx.scene_viewport_size.x > 1.0f && ctx.scene_viewport_size.y > 1.0f)
            m_editor_camera.setViewportSize(ctx.scene_viewport_size.x, ctx.scene_viewport_size.y);

        const bool camera_input_active = ctx.scene_viewport_image_hovered;
        const InputState& input = m_services.input->getState();

        const float mdx = camera_input_active ? input.getMouseDeltaX() : 0.0f;
        const float mdy = camera_input_active ? input.getMouseDeltaY() : 0.0f;
        const float scroll_y = camera_input_active ? input.getScrollDeltaY() : 0.0f;

        const bool lmb_down = camera_input_active && input.isMouseDown(GLFW_MOUSE_BUTTON_LEFT);
        const bool mmb_down = camera_input_active && input.isMouseDown(GLFW_MOUSE_BUTTON_MIDDLE);
        const bool rmb_down = camera_input_active && input.isMouseDown(GLFW_MOUSE_BUTTON_RIGHT);
        const bool mmb_for_camera = mmb_down || (ctx.pan_tool && lmb_down);

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

        ctx.suppress_tool_shortcuts = camera_input_active && rmb_down && !key_alt;

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
