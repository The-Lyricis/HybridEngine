#include "scene_view_interaction.h"

#include "editor/core/editor_drag_drop.h"

#include "runtime/core/base/macro.h"

namespace Hybrid
{
    void HandleSceneViewDragDrop(EditorContext& ctx, const char* log_tag)
    {
        if (!ImGui::BeginDragDropTarget())
            return;

        const ImVec2 drop_mouse_pos = ImGui::GetMousePos();
        AssetID dropped_asset{};
        if (ctx.scene_actions.instantiate_asset && EditorDragDrop::AcceptAsset(dropped_asset))
        {
            HBD_CORE_INFO("{} asset_drop_requested asset_id={} x={} y={}",
                          log_tag,
                          dropped_asset.value,
                          drop_mouse_pos.x,
                          drop_mouse_pos.y);
            (void)ctx.scene_actions.instantiate_asset(dropped_asset, drop_mouse_pos);
        }

        std::string dropped_rel_path;
        if (ctx.scene_actions.instantiate_project_path && EditorDragDrop::AcceptProjectPath(dropped_rel_path))
        {
            HBD_CORE_INFO("{} project_path_drop_requested path={} x={} y={}",
                          log_tag,
                          dropped_rel_path,
                          drop_mouse_pos.x,
                          drop_mouse_pos.y);
            (void)ctx.scene_actions.instantiate_project_path(dropped_rel_path, drop_mouse_pos);
        }

        ImGui::EndDragDropTarget();
    }

    void HandleSceneViewPicking(EditorContext& ctx,
                                const ImVec2& viewport_min,
                                const ImVec2& canvas_size,
                                bool toolbar_interacted,
                                bool gizmo_using,
                                bool gizmo_over,
                                const char* log_tag)
    {
        if (!ctx.scene_viewport.image_hovered ||
            !ImGui::IsMouseClicked(ImGuiMouseButton_Left) ||
            toolbar_interacted ||
            gizmo_using ||
            gizmo_over)
        {
            return;
        }

        const ImVec2 mouse = ImGui::GetMousePos();
        const int px = static_cast<int>(mouse.x - viewport_min.x);
        const int py = static_cast<int>(canvas_size.y - 1 - (mouse.y - viewport_min.y));

        if (px < 0 || py < 0 || px >= static_cast<int>(canvas_size.x) || py >= static_cast<int>(canvas_size.y))
            return;

        ctx.picking.request = true;
        ctx.picking.x = px;
        ctx.picking.y = py;
        ctx.picking.toggle = ImGui::GetIO().KeyCtrl;
        HBD_CORE_DEBUG("{} pick_requested x={} y={} toggle={}",
                       log_tag,
                       px,
                       py,
                       ctx.picking.toggle);
    }
} // namespace Hybrid
