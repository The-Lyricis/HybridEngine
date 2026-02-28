#include "editor_layer.h"

#include <entt/entity/entity.hpp>
#include <system_error>
#include <utility>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "editor/core/editor_context.h"
#include "editor/services/asset/editor_resource_system.h"
#include "runtime/core/base/macro.h"
#include "runtime/modules/asset/runtime_resource_system.h"
#include "runtime/modules/input/input_layer.h"
#include "runtime/modules/render/runtime/editor_render_ext.h"
#include "runtime/modules/render/runtime/frame_context.h"
#include "runtime/modules/render/runtime/render_flags.h"
#include "runtime/modules/render/runtime/render_system.h"
#include "runtime/modules/scene/scene_manager.h"
#include "runtime/modules/window/window_system.h"
#include "runtime/modules/scene/scene_serializer.h"
#include "runtime/modules/scene/scene.h"

namespace Hybrid
{
    EditorLayer::EditorLayer(EngineServices services) : Layer("EditorLayer"), m_services(std::move(services)) {}
    void EditorLayer::onAttach()
    {
        if (!m_services.window || !m_services.render || !m_services.scene ||
            !m_services.frame_context || !m_services.render_flags || !m_services.editor_ext)
        {
            HBD_CORE_ERROR("EditorLayer attach failed: missing required engine services");
            return;
        }

        m_editor_ui.initialize(m_services.window->getNativeWindow());
        bindAssetChangeCallback();

        if (m_services.resources && m_services.resources->getRegistry())
        {
            m_assets_root = m_services.resources->getRegistry()->getRoot();
            if (!m_assets_root.empty())
            {
                if (!m_file_watcher.initialize(m_assets_root, true))
                {
                    HBD_CORE_WARN("EditorLayer: file watcher init failed at {}", m_assets_root.string());
                }
            }
        }

        if (m_services.editor_resources)
        {
            m_services.editor_resources->bootstrapImportOnce();
        }
        m_initialized = true;

        auto& ctx = m_editor_ui.context();
        ctx.open_scene = [this](const std::string& scene_vpath)
            {
                if (!m_services.scene || !m_services.render || !m_services.resources)
                    return;

                auto vfs = m_services.resources->getVFS();
                if (!vfs)
                {
                    HBD_CORE_WARN("EditorLayer: open_scene failed, VFS is null");
                    return;
                }

                auto native = vfs->resolve(scene_vpath);
                if (!native)
                {
                    HBD_CORE_WARN("EditorLayer: open_scene resolve failed: {}", scene_vpath);
                    return;
                }

                auto new_scene = std::make_shared<Scene>();
                if (!SceneSerializer::DeserializeFromFile(*new_scene, *native))
                {
                    HBD_CORE_WARN("EditorLayer: open_scene deserialize failed: {}", native->string());
                    return;
                }

                // 切换场景（这是关键）
                m_services.scene->setActiveScene(new_scene);
                m_services.render->setScene(new_scene);

                // 同步 EditorContext
                m_editor_ui.setActiveScene(new_scene.get());
                m_editor_ui.context().active_scene = new_scene.get();
                m_editor_ui.context().selected = entt::null;

                HBD_CORE_INFO("EditorLayer: opened scene {}", scene_vpath);
            };
    }

    void EditorLayer::onDetach()
    {
        if (!m_initialized)
            return;

        // 先断开所有 EditorContext 回调，避免 shutdown 期间触发 use-after-free
        auto& ctx = m_editor_ui.context();
        ctx.notify_asset_source_event = {};
        ctx.open_scene = {};                

        // 还可以顺便把选择清空，避免其他地方读到野值
        ctx.selected = entt::null;
        ctx.active_scene = nullptr;

        if (m_services.editor_ext)
            m_services.editor_ext->has_editor_camera = false;

        m_editor_ui.shutdown();
        m_initialized = false;
    }

    void EditorLayer::onUpdate(float dt)
    {
        if (!m_initialized)
            return;

        m_editor_ui.updateViewportState();
        updateEditorCamera(dt);
        pollFileWatcher();

        if (m_services.editor_resources)
        {
            m_services.editor_resources->processImportQueue(4, 2);
        }

        if (m_services.consume_pick_result)
        {
            uint32_t picked = 0;
            if (m_services.consume_pick_result(picked))
            {
                auto& ctx = m_editor_ui.context();
                ctx.selected = (picked == 0) ? entt::null : static_cast<entt::entity>(picked);
            }
        }

        // Pre-render sync: publish this frame's camera/render inputs.
        updateFrameContext();
    }

    void EditorLayer::onImGuiRender()
    {
        if (!m_initialized)
            return;

        auto active_scene = m_services.scene->getActiveScene();
        m_editor_ui.setActiveScene(active_scene.get());

        auto& ctx = m_editor_ui.context();
        ctx.active_scene = active_scene.get();
        ctx.gizmo_view = m_services.render->getLastView();
        ctx.gizmo_proj = m_services.render->getLastProj();

        m_editor_ui.drawPanels();
        m_editor_ui.drawViewport(m_services.render->getSceneColorTexture());
    }

    void EditorLayer::updateFrameContext()
    {
        auto* frame_context = m_services.frame_context;
        auto* render_flags = m_services.render_flags;
        auto* editor_ext = m_services.editor_ext;
        if (!frame_context || !render_flags || !editor_ext)
            return;

        auto& ctx = m_editor_ui.context();
        frame_context->viewport_size = {ctx.viewport_size.x, ctx.viewport_size.y};
        editor_ext->viewport_active = ctx.viewport_image_hovered;
        editor_ext->use_game_camera = ctx.use_game_camera;
        editor_ext->selected_entity_id =
            (ctx.selected == entt::null) ? 0u : static_cast<uint32_t>(entt::to_integral(ctx.selected));
        editor_ext->pan_tool = ctx.pan_tool;
        if (!ctx.use_game_camera)
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
        if (editor_ext->selected_entity_id != 0)
        {
            *render_flags |= RenderFlags::SelectionOutline;
        }

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

    void EditorLayer::updateEditorCamera(float dt)
    {
        if (!m_services.input || !m_services.editor_ext)
            return;

        auto& ctx = m_editor_ui.context();
        if (ctx.viewport_size.x > 1.0f && ctx.viewport_size.y > 1.0f)
        {
            m_editor_camera.setViewportSize(ctx.viewport_size.x, ctx.viewport_size.y);
        }

        const bool camera_input_active = !ctx.use_game_camera && ctx.viewport_image_hovered;
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

    void EditorLayer::bindAssetChangeCallback()
    {
        if (!m_services.editor_resources)
            return;

        auto& ctx = m_editor_ui.context();
        ctx.notify_asset_source_event = [this](const AssetSourceEvent& event) {
            if (!m_services.editor_resources)
                return;

            switch (event.type)
            {
            case AssetSourceEventType::Added:
                m_services.editor_resources->enqueueSourceChanged(event.path, AssetSourceChangeType::Added);
                break;
            case AssetSourceEventType::Modified:
                m_services.editor_resources->enqueueSourceChanged(event.path, AssetSourceChangeType::Modified);
                break;
            case AssetSourceEventType::Removed:
                m_services.editor_resources->enqueueSourceChanged(event.path, AssetSourceChangeType::Removed);
                break;
            case AssetSourceEventType::Moved:
                (void)m_services.editor_resources->moveAsset(event.old_path, event.new_path);
                break;
            default:
                break;
            }
        };
    }

    void EditorLayer::pollFileWatcher()
    {
        if (!m_services.editor_resources || !m_file_watcher.isInitialized())
            return;

        m_file_watcher.poll([this](const std::filesystem::path& physical_path, FileWatcherChangeType type) {
            std::string source_vpath;
            if (!toAssetVPath(physical_path, source_vpath))
                return;

            const AssetSourceChangeType change =
                (type == FileWatcherChangeType::Removed)
                    ? AssetSourceChangeType::Removed
                    : (type == FileWatcherChangeType::Added ? AssetSourceChangeType::Added
                                                            : AssetSourceChangeType::Modified);
            m_services.editor_resources->enqueueSourceChanged(source_vpath, change);
        });
    }

    bool EditorLayer::toAssetVPath(const std::filesystem::path& physical_path, std::string& out_vpath) const
    {
        out_vpath.clear();
        if (m_assets_root.empty() || physical_path.empty())
            return false;

        std::error_code ec;
        auto rel = std::filesystem::relative(physical_path, m_assets_root, ec);
        if (ec || rel.empty())
            return false;

        std::string rel_str = rel.generic_string();
        while (!rel_str.empty() && (rel_str.front() == '/' || rel_str.front() == '\\'))
            rel_str.erase(rel_str.begin());
        if (rel_str.empty())
            return false;

        out_vpath = std::string("asset:") + rel_str;
        return true;
    }
} // namespace Hybrid
