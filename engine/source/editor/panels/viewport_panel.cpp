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

        // Image 的屏幕矩形已经写入 ctx.viewport_min/max
        // 现在检测点击 Image 本身
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
        {
            ImGuiIO& io = ImGui::GetIO();

            float mx = io.MousePos.x - ctx.viewport_min.x;
            float my = io.MousePos.y - ctx.viewport_min.y;

            // 转为渲染目标像素坐标（OpenGL 原点在左下）
            int px = (int)mx;
            int py = (int)(ctx.viewport_size.y - my - 1);

            // clamp
            px = std::max(0, std::min(px, (int)ctx.viewport_size.x - 1));
            py = std::max(0, std::min(py, (int)ctx.viewport_size.y - 1));

            ctx.pick_x = px;
            ctx.pick_y = py;
            ctx.request_pick = true;
        }

        // 记录 Image 在屏幕上的矩形（用于下一步 Picking / Gizmo）
        ctx.viewport_min = ImGui::GetItemRectMin();
        ctx.viewport_max = ImGui::GetItemRectMax();

        ImGui::End();
    }
}
