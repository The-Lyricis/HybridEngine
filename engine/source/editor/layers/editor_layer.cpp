#include "editor_layer.h"

#include <entt/entity/entity.hpp>
#include <utility>

#include "editor/editor_context.h"
#include "runtime/core/base/macro.h"
#include "runtime/function/render/editor_render_ext.h"
#include "runtime/function/render/frame_context.h"
#include "runtime/function/render/render_flags.h"
#include "runtime/function/window/window_system.h"
#include "runtime/function/scene/scene_manager.h"
#include "runtime/function/render/render_system.h"

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
        m_initialized = true;
    }

    void EditorLayer::onDetach()
    {
        if (!m_initialized)
            return;

        m_editor_ui.shutdown();
        m_initialized = false;
    }

    void EditorLayer::onUpdate(float /*dt*/)
    {
        if (!m_initialized || !m_services.consume_pick_result)
            return;

        uint32_t picked = 0;
        if (!m_services.consume_pick_result(picked))
            return;

        auto& ctx = m_editor_ui.context();
        ctx.selected = (picked == 0) ? entt::null : static_cast<entt::entity>(picked);
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

        updateFrameContext();
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
} // namespace Hybrid
