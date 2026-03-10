#include "game_view_panel.h"

#include "editor/core/editor_context.h"

#include <imgui.h>

namespace Hybrid
{
    void GameViewPanel::updateViewportState(EditorContext& ctx)
    {
        if (!m_open)
        {
            ctx.game_viewport_image_hovered = false;
            ctx.game_viewport_hovered = false;
            ctx.game_viewport_focused = false;
            return;
        }

        const bool has_canvas_rect =
            (ctx.game_viewport_max.x > ctx.game_viewport_min.x) &&
            (ctx.game_viewport_max.y > ctx.game_viewport_min.y);
        const bool hovered = has_canvas_rect &&
            ImGui::IsMouseHoveringRect(ctx.game_viewport_min, ctx.game_viewport_max, false);

        ctx.game_viewport_image_hovered = hovered;
        ctx.game_viewport_hovered = hovered;

        if (hovered &&
            (ImGui::IsMouseClicked(ImGuiMouseButton_Left) ||
             ImGui::IsMouseClicked(ImGuiMouseButton_Middle) ||
             ImGui::IsMouseClicked(ImGuiMouseButton_Right)))
        {
            ctx.game_viewport_focused = true;
        }
        else if (!hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            ctx.game_viewport_focused = false;
        }
    }

    void GameViewPanel::onImGuiRender(EditorContext& ctx)
    {
        if (!m_open)
            return;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{0, 0});
        ImGui::Begin(getName(), &m_open);

        const ImVec2 canvas_min = ImGui::GetCursorScreenPos();
        ImVec2 canvas_size = ImGui::GetContentRegionAvail();
        if (canvas_size.x < 1.0f) canvas_size.x = 1.0f;
        if (canvas_size.y < 1.0f) canvas_size.y = 1.0f;
        const ImVec2 canvas_max = {canvas_min.x + canvas_size.x, canvas_min.y + canvas_size.y};

        ImGui::GetWindowDrawList()->AddImage(
            (ImTextureID)(intptr_t)m_colorTextureID, canvas_min, canvas_max, {0, 1}, {1, 0});

        ctx.game_viewport_image_hovered = ImGui::IsMouseHoveringRect(canvas_min, canvas_max, false);
        ctx.game_viewport_hovered = ctx.game_viewport_image_hovered;
        ctx.game_viewport_focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
        ctx.game_viewport_size = canvas_size;
        ctx.game_viewport_min = canvas_min;
        ctx.game_viewport_max = canvas_max;

        ImGui::End();
        ImGui::PopStyleVar();
    }
} // namespace Hybrid
