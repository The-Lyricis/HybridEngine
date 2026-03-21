#include "game_view_panel.h"

#include "editor/core/editor_context.h"
#include "runtime/core/base/macro.h"

#include <imgui.h>

namespace Hybrid
{
    namespace
    {
        constexpr const char* kGameViewPanelLogTag = "[GameViewPanel]";
    } // namespace

    void GameViewPanel::updateViewportState(EditorContext& ctx)
    {
        if (!m_open)
        {
            ctx.game_viewport_image_hovered = false;
            ctx.game_viewport_hovered = false;
            ctx.game_viewport_focused = false;
        }
    }

    void GameViewPanel::onImGuiRender(EditorContext& ctx)
    {
        if (!m_open)
            return;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{0, 0});
        ImGui::Begin(getName(), &m_open);

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

        ctx.game_viewport_image_hovered = viewport_hovered;
        ctx.game_viewport_hovered = viewport_hovered;
        ctx.game_viewport_focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
        ctx.game_viewport_size = canvas_size;
        ctx.game_viewport_min = viewport_min;
        ctx.game_viewport_max = viewport_max;

        ImGui::End();
        ImGui::PopStyleVar();
    }
} // namespace Hybrid
