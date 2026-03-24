#include "scene_view_viewport.h"

#include <imgui.h>

#include "runtime/core/base/macro.h"

namespace Hybrid
{
    SceneViewViewportResult DrawSceneViewViewport(EditorContext& ctx,
                                                  uint32_t color_texture_id,
                                                  bool toolbar_interacted,
                                                  SceneViewViewportState& state,
                                                  const char* log_tag)
    {
        if (color_texture_id == 0)
        {
            if (!state.missing_texture_logged)
            {
                HBD_CORE_WARN("{} viewport_texture_missing", log_tag);
                state.missing_texture_logged = true;
            }
        }
        else if (state.missing_texture_logged)
        {
            HBD_CORE_INFO("{} viewport_texture_ready texture_id={}", log_tag, color_texture_id);
            state.missing_texture_logged = false;
        }

        const ImVec2 canvas_min = ImGui::GetCursorScreenPos();

        SceneViewViewportResult result;
        result.canvas_size = ImGui::GetContentRegionAvail();
        if (result.canvas_size.x < 1.0f)
            result.canvas_size.x = 1.0f;
        if (result.canvas_size.y < 1.0f)
            result.canvas_size.y = 1.0f;

        ImGui::SetCursorScreenPos(canvas_min);
        ImGui::Image((ImTextureID)(intptr_t)color_texture_id, result.canvas_size, ImVec2(0, 1), ImVec2(1, 0));

        result.viewport_min = ImGui::GetItemRectMin();
        result.viewport_max = ImGui::GetItemRectMax();
        result.hovered = ImGui::IsItemHovered();

        ctx.scene_viewport.image_hovered = result.hovered;
        ctx.scene_viewport.hovered = toolbar_interacted ? false : result.hovered;
        ctx.scene_viewport.focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
        ctx.scene_viewport.size = result.canvas_size;
        ctx.scene_viewport.min = result.viewport_min;
        ctx.scene_viewport.max = result.viewport_max;

        return result;
    }
} // namespace Hybrid
