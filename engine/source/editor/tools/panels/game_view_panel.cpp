#include "game_view_panel.h"

#include "editor/core/context/editor_context.h"
#include "runtime/core/base/macro.h"
#include "runtime/modules/render/runtime/render_system.h"

#include <imgui.h>

#include <cstdio>
#include <iterator>

namespace Hybrid
{
    namespace
    {
        constexpr const char* kGameViewPanelLogTag = "[GameViewPanel]";
    } // namespace

    void GameViewPanel::updateViewportState(EditorContext& ctx)
    {
        if (!m_state.open)
        {
            ctx.game_viewport.image_hovered = false;
            ctx.game_viewport.hovered = false;
            ctx.game_viewport.focused = false;
        }
    }

    void GameViewPanel::onImGuiRender(EditorContext& ctx)
    {
        if (!m_state.open)
            return;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{0, 0});
        ImGui::Begin(getName(), &m_state.open);

        if (m_colorTextureID == 0)
        {
            if (!m_missingTextureLogged)
            {
                HBD_CORE_WARN("{} viewport_texture_missing", kGameViewPanelLogTag);
                m_missingTextureLogged = true;
            }
        }
        else if (m_missingTextureLogged)
        {
            HBD_CORE_INFO("{} viewport_texture_ready texture_id={}", kGameViewPanelLogTag, m_colorTextureID);
            m_missingTextureLogged = false;
        }

        const ImVec2 canvas_min = ImGui::GetCursorScreenPos();
        ImVec2 canvas_size = ImGui::GetContentRegionAvail();
        if (canvas_size.x < 1.0f) canvas_size.x = 1.0f;
        if (canvas_size.y < 1.0f) canvas_size.y = 1.0f;

        ImGui::SetCursorScreenPos(canvas_min);
        ImGui::Image((ImTextureID)(intptr_t)m_colorTextureID, canvas_size, ImVec2(0, 1), ImVec2(1, 0));

        const ImVec2 viewport_min = ImGui::GetItemRectMin();
        const ImVec2 viewport_max = ImGui::GetItemRectMax();
        const bool viewport_hovered = ImGui::IsItemHovered();

        if (ctx.debug.render_stats != nullptr)
        {
            ImDrawList* draw_list = ImGui::GetWindowDrawList();
            char line0[64] = {};
            char line1[64] = {};
            char line2[64] = {};
            char line3[64] = {};
            char line4[64] = {};

            std::snprintf(line0,
                          sizeof(line0),
                          "Frame %.1f FPS | %.2f ms",
                          ctx.debug.render_stats->fps,
                          ctx.debug.render_stats->frame_time_ms);
            std::snprintf(line1,
                          sizeof(line1),
                          "Render %.2f ms | Draws %u",
                          ctx.debug.render_stats->render_cpu_time_ms,
                          ctx.debug.render_stats->submitted_draw_calls);
            std::snprintf(line2,
                          sizeof(line2),
                          "Submitted O %u | T %u | Tris %u",
                          ctx.debug.render_stats->submitted_opaque_items,
                          ctx.debug.render_stats->submitted_transparent_items,
                          ctx.debug.render_stats->submitted_triangles);
            std::snprintf(line3,
                          sizeof(line3),
                          "Cull Tested %u | Culled %u | Shadow %u",
                          ctx.debug.render_stats->tested_items,
                          ctx.debug.render_stats->culled_items,
                          ctx.debug.render_stats->shadow_caster_items);
            std::snprintf(line4,
                          sizeof(line4),
                          "Scene Renderers %u | Submeshes %u | Lights %u",
                          ctx.debug.render_stats->scene_renderers,
                          ctx.debug.render_stats->scene_submeshes,
                          ctx.debug.render_stats->point_lights);

            const char* lines[] = {line0, line1, line2, line3, line4};
            float max_width = 0.0f;
            for (const char* line : lines)
                max_width = std::max(max_width, ImGui::CalcTextSize(line).x);

            const float line_height = ImGui::GetTextLineHeight();
            const ImVec2 padding(8.0f, 6.0f);
            const ImVec2 panel_min(viewport_max.x - max_width - padding.x * 2.0f - 8.0f,
                                   viewport_min.y + 8.0f);
            const ImVec2 panel_max(viewport_max.x - 8.0f,
                                   panel_min.y + line_height * static_cast<float>(std::size(lines)) + padding.y * 2.0f);

            draw_list->AddRectFilled(panel_min, panel_max, IM_COL32(18, 22, 28, 190), 6.0f);
            for (int i = 0; i < static_cast<int>(std::size(lines)); ++i)
            {
                const ImVec2 text_pos(panel_min.x + padding.x, panel_min.y + padding.y + line_height * static_cast<float>(i));
                draw_list->AddText(text_pos, ImGui::GetColorU32(ImGuiCol_Text), lines[i]);
            }
        }

        ctx.game_viewport.image_hovered = viewport_hovered;
        ctx.game_viewport.hovered = viewport_hovered;
        ctx.game_viewport.focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
        ctx.game_viewport.size = canvas_size;
        ctx.game_viewport.min = viewport_min;
        ctx.game_viewport.max = viewport_max;

        ImGui::End();
        ImGui::PopStyleVar();
    }
} // namespace Hybrid
