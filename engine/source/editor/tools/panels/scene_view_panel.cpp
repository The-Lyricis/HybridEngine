#include "scene_view_panel.h"

#include "editor/core/context/editor_context.h"
#include "editor/tools/panels/scene/scene_view_gizmo.h"
#include "editor/tools/panels/scene/scene_view_interaction.h"
#include "editor/tools/panels/scene/scene_view_viewport.h"
#include "editor/tools/panels/scene/scene_view_toolbar.h"

#include <imgui.h>

namespace Hybrid
{
    namespace
    {
        constexpr const char* kSceneViewPanelLogTag = "[SceneViewPanel]";
    }

    void SceneViewPanel::updateViewportState(EditorContext& ctx)
    {
        if (!m_state.open)
        {
            ctx.scene_viewport.image_hovered = false;
            ctx.scene_viewport.hovered = false;
            ctx.scene_viewport.focused = false;
        }
    }

    void SceneViewPanel::onImGuiRender(EditorContext& ctx)
    {
        if (!m_state.open)
            return;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{0, 0});
        ImGui::Begin(getName(), &m_state.open);

        const SceneViewToolbarResult toolbar = DrawSceneViewToolbar(ctx, ImGui::GetContentRegionAvail().x);
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - ImGui::GetStyle().ItemSpacing.y);

        const SceneViewViewportResult viewport =
            DrawSceneViewViewport(ctx, m_colorTextureID, toolbar.interacted, m_viewport_state, kSceneViewPanelLogTag);

        HandleSceneViewDragDrop(ctx, kSceneViewPanelLogTag);

        const SceneViewGizmoResult gizmo =
            HandleSceneViewGizmo(ctx, viewport.viewport_min, viewport.canvas_size, m_gizmo_drag_state);

        HandleSceneViewPicking(
            ctx,
            viewport.viewport_min,
            viewport.canvas_size,
            toolbar.interacted,
            gizmo.using_gizmo,
            gizmo.gizmo_over,
            kSceneViewPanelLogTag);

        ImGui::End();
        ImGui::PopStyleVar();
    }
} // namespace Hybrid
