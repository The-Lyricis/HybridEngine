#include "viewport_panel.h"
#include "../editor_context.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <ImGuizmo.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/euler_angles.hpp>

#include "runtime/function/scene/scene.h"
#include "runtime/function/scene/components.h"

namespace Hybrid
{
    static glm::mat4 BuildModel(const TransformComponent& tr)
    {
        glm::mat4 T = glm::translate(glm::mat4(1.0f), tr.Position);
        glm::mat4 R = glm::yawPitchRoll(tr.Rotation.y, tr.Rotation.x, tr.Rotation.z);
        glm::mat4 S = glm::scale(glm::mat4(1.0f), tr.Scale);
        return T * R * S;
    }

    enum class ToolMode { Hand, Move, Rotate, Scale };
    static ToolMode s_Tool = ToolMode::Move;
    static ImGuizmo::MODE s_GizmoSpace = ImGuizmo::LOCAL;

    static void PushToolButtonStyle(bool active)
    {
        if (!active) return;
        // 简单高亮：更接近 Unity 的“选中态”
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.45f, 0.95f, 0.90f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.45f, 0.95f, 1.00f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.20f, 0.40f, 0.90f, 1.00f));
    }

    static void PopToolButtonStyle(bool active)
    {
        if (!active) return;
        ImGui::PopStyleColor(3);
    }

    // 返回：是否点击了工具栏（用于屏蔽 picking）
    static bool DrawUnityToolBar(const ImVec2& canvasMin)
    {
        bool clicked = false;

        // Unity 快捷键：Q/W/E/R
        if (ImGui::IsKeyPressed(ImGuiKey_Q)) s_Tool = ToolMode::Hand;
        if (ImGui::IsKeyPressed(ImGuiKey_W)) s_Tool = ToolMode::Move;
        if (ImGui::IsKeyPressed(ImGuiKey_E)) s_Tool = ToolMode::Rotate;
        if (ImGui::IsKeyPressed(ImGuiKey_R)) s_Tool = ToolMode::Scale;

        ImGui::SetNextWindowPos(ImVec2(canvasMin.x + 8.0f, canvasMin.y + 8.0f));
        ImGui::SetNextWindowBgAlpha(0.55f);

        ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoDocking |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoFocusOnAppearing |
            ImGuiWindowFlags_NoNav;

        ImGui::Begin("##UnityTools", nullptr, flags);

        const ImVec2 btnSize(34.0f, 34.0f);

        auto ToolButton = [&](const char* icon, const char* tip, ToolMode mode)
            {
                const bool active = (s_Tool == mode);
                PushToolButtonStyle(active);
                if (ImGui::Button(icon, btnSize))
                {
                    s_Tool = mode;
                    clicked = true;
                }
                PopToolButtonStyle(active);

                if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
                    ImGui::SetTooltip("%s", tip);
            };

        // 图标占位（你可换成 FontAwesome）
        ToolButton("🖐", "Hand (Q)\nPan / Navigate", ToolMode::Hand);
        ToolButton("⇄", "Move (W)", ToolMode::Move);
        ToolButton("⟳", "Rotate (E)", ToolMode::Rotate);
        ToolButton("⬚", "Scale (R)", ToolMode::Scale);

        ImGui::Separator();

        // Local/World 切换（Unity 在旋转/移动常用）
        if (s_Tool != ToolMode::Scale)
        {
            const bool isLocal = (s_GizmoSpace == ImGuizmo::LOCAL);
            if (ImGui::Button(isLocal ? "Local" : "World", ImVec2(btnSize.x * 1.0f, 0)))
            {
                s_GizmoSpace = isLocal ? ImGuizmo::WORLD : ImGuizmo::LOCAL;
                clicked = true;
            }
        }

        ImGui::End();
        return clicked;
    }

    void ViewportPanel::onImGuiRender(EditorContext& ctx)
    {
        if (!m_open) return;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0, 0 });
        ImGui::Begin("Viewport", &m_open);

        // --- 顶部相机模式 ---
        {
            ImGui::SetCursorPosX(5.0f);
            ImGui::TextUnformatted("Camera:");
            ImGui::SameLine();
            ImGui::TextUnformatted(ctx.use_game_camera ? "Game" : "Editor");
            ImGui::SameLine();

            if (ImGui::Button(ctx.use_game_camera ? "Switch to Editor Camera" : "Switch to Game Camera"))
                ctx.use_game_camera = !ctx.use_game_camera;

            ImGui::Separator();
        }

        // --- 画面区域 ---
        ImVec2 canvasMin = ImGui::GetCursorScreenPos();
        ImVec2 canvasSize = ImGui::GetContentRegionAvail();
        if (canvasSize.x < 1.0f) canvasSize.x = 1.0f;
        if (canvasSize.y < 1.0f) canvasSize.y = 1.0f;
        ImVec2 canvasMax = { canvasMin.x + canvasSize.x, canvasMin.y + canvasSize.y };

        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddImage((ImTextureID)(intptr_t)m_colorTextureID, canvasMin, canvasMax, { 0, 1 }, { 1, 0 });

        // hover/focus（仅画面区域）
        ctx.viewport_image_hovered = ImGui::IsMouseHoveringRect(canvasMin, canvasMax, false);
        ctx.viewport_hovered = ctx.viewport_image_hovered;
        ctx.viewport_focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

        ctx.viewport_size = canvasSize;
        ctx.viewport_min = canvasMin;
        ctx.viewport_max = canvasMax;

        // Game camera：禁用 gizmo（按你之前需求）
        if (ctx.use_game_camera)
        {
            ctx.gizmo_using = false;
            ctx.request_pick = false;
            ImGui::End();
            ImGui::PopStyleVar();
            return;
        }

        // --- Unity 风格工具栏 ---
        const bool toolbarClicked = DrawUnityToolBar(canvasMin);

        // --- Gizmo ---
        ctx.pan_tool = (s_Tool == ToolMode::Hand);
        bool gizmoOver = false;
        ctx.gizmo_using = false;

        // Hand 模式：不显示 gizmo（但允许选中与相机导航）
        const bool wantGizmo =
            (s_Tool == ToolMode::Move || s_Tool == ToolMode::Rotate || s_Tool == ToolMode::Scale);

        if (wantGizmo && ctx.active_scene && ctx.selected != entt::null)
        {
            auto& reg = ctx.active_scene->getRegistry();
            if (reg.valid(ctx.selected) && reg.all_of<TransformComponent>(ctx.selected))
            {
                auto& tr = reg.get<TransformComponent>(ctx.selected);
                glm::mat4 model = BuildModel(tr);

                ImGuizmo::SetOrthographic(false);
                ImGuizmo::SetDrawlist(); // 你的版本可用
                ImGuizmo::SetRect(canvasMin.x, canvasMin.y, canvasSize.x, canvasSize.y);

                ImGuizmo::OPERATION op = ImGuizmo::TRANSLATE;
                if (s_Tool == ToolMode::Rotate) op = ImGuizmo::ROTATE;
                if (s_Tool == ToolMode::Scale)  op = ImGuizmo::SCALE;

                // Snap（Unity：Ctrl 吸附）
                bool snapping = ImGui::GetIO().KeyCtrl;
                float snap[3] = { 0.5f, 0.5f, 0.5f };
                if (op == ImGuizmo::ROTATE) { snap[0] = 15.0f; snap[1] = 15.0f; snap[2] = 15.0f; }
                if (op == ImGuizmo::SCALE) { snap[0] = 0.1f;  snap[1] = 0.1f;  snap[2] = 0.1f; }

                ImGuizmo::Manipulate(
                    glm::value_ptr(ctx.gizmo_view),
                    glm::value_ptr(ctx.gizmo_proj),
                    op,
                    (op == ImGuizmo::SCALE) ? ImGuizmo::LOCAL : s_GizmoSpace,
                    glm::value_ptr(model),
                    nullptr,
                    snapping ? snap : nullptr
                );

                ctx.gizmo_using = ImGuizmo::IsUsing();
                gizmoOver = ImGuizmo::IsOver();

                if (ctx.gizmo_using)
                {
                    float t[3], rDeg[3], s[3];
                    ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(model), t, rDeg, s);

                    tr.Position = { t[0], t[1], t[2] };
                    tr.Rotation = { glm::radians(rDeg[0]), glm::radians(rDeg[1]), glm::radians(rDeg[2]) };
                    tr.Scale = { s[0], s[1], s[2] };
                }
            }
        }

        // --- Picking：工具栏点击 / gizmo hit / gizmo using 时不触发 ---
        if (ctx.viewport_image_hovered &&
            ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
            !toolbarClicked &&
            !ctx.gizmo_using &&
            !gizmoOver)
        {
            ImVec2 mouse = ImGui::GetMousePos();
            int px = (int)(mouse.x - canvasMin.x);
            int py = (int)(canvasSize.y - 1 - (mouse.y - canvasMin.y)); // Y flip

            if (px >= 0 && py >= 0 && px < (int)canvasSize.x && py < (int)canvasSize.y)
            {
                ctx.request_pick = true;
                ctx.pick_x = px;
                ctx.pick_y = py;
            }
        }

        ImGui::End();
        ImGui::PopStyleVar();
    }

} // namespace Hybrid
