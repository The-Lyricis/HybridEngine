#include "viewport_panel.h"
#include "../editor_context.h"
#include <imgui.h>

namespace Hybrid
{
    void ViewportPanel::onImGuiRender(EditorContext& ctx)
    {
        if (!m_open) return;

        ImGui::Begin(getName(), &m_open);

        // --- Toolbar: Camera Mode Toggle ---
        ImGui::TextUnformatted("Camera:");
        ImGui::SameLine();
        ImGui::TextUnformatted(ctx.use_game_camera ? "Game" : "Editor");
        ImGui::SameLine();
        if (ImGui::Button(ctx.use_game_camera ? "Switch to Editor Camera" : "Switch to Game Camera"))
            ctx.use_game_camera = !ctx.use_game_camera;

        ImGui::Separator();

        ctx.viewport_focused = ImGui::IsWindowFocused();
        ctx.viewport_hovered = ImGui::IsWindowHovered();

        ImVec2 avail = ImGui::GetContentRegionAvail();
        if (avail.x < 1.0f) avail.x = 1.0f;
        if (avail.y < 1.0f) avail.y = 1.0f;
        ctx.viewport_size = avail;

        // 画视口纹理
        ImGui::Image((ImTextureID)(intptr_t)m_colorTextureID, avail, ImVec2(0, 1), ImVec2(1, 0));

        // 记录 Image 在屏幕上的矩形（用于下一步 Picking / Gizmo）
        ctx.viewport_min = ImGui::GetItemRectMin();
        ctx.viewport_max = ImGui::GetItemRectMax();

        ImGui::End();
    }
}
