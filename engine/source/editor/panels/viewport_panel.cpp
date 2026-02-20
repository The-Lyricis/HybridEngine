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

    // 仅用于“平移模式”的稳定回写：只取 translation，避免分解旋转/缩放带来的抖动
    static void ApplyTranslationOnly(TransformComponent& tr, const glm::mat4& model)
    {
        const glm::vec3 t = glm::vec3(model[3]); // column-major: 4th column is translation
        tr.Position = t;
    }

    void ViewportPanel::onImGuiRender(EditorContext& ctx)
    {
        if (!m_open) return;

        // 1. 样式调整：让内容铺满窗口
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0, 0 });
        ImGui::Begin("Viewport", &m_open);

        // --- Toolbar: Camera Mode Toggle (保留原有逻辑) ---
        {
            // 工具栏需要一点内边距才好看，这里手动偏移一下
            ImGui::SetCursorPosX(5.0f);
            ImGui::TextUnformatted("Camera:");
            ImGui::SameLine();
            ImGui::TextUnformatted(ctx.use_game_camera ? "Game" : "Editor");
            ImGui::SameLine();

            if (ImGui::Button(ctx.use_game_camera ? "Switch to Editor Camera" : "Switch to Game Camera"))
                ctx.use_game_camera = !ctx.use_game_camera;

            ImGui::Separator();
        }

        // 获取视口基础信息
        ImVec2 canvasMin = ImGui::GetCursorScreenPos();
        ImVec2 canvasSize = ImGui::GetContentRegionAvail();
        if (canvasSize.x < 1.0f) canvasSize.x = 1.0f;
        if (canvasSize.y < 1.0f) canvasSize.y = 1.0f;
        ImVec2 canvasMax = { canvasMin.x + canvasSize.x, canvasMin.y + canvasSize.y };

        // 绘制背景底图
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddImage((ImTextureID)(intptr_t)m_colorTextureID, canvasMin, canvasMax, { 0, 1 }, { 1, 0 });

        ctx.viewport_image_hovered = ImGui::IsMouseHoveringRect(canvasMin, canvasMax, false);

        // 更新 Context 状态
        ctx.viewport_size = canvasSize;
        ctx.viewport_min = canvasMin;
        ctx.viewport_focused = ImGui::IsWindowFocused();
        ctx.viewport_hovered = ImGui::IsWindowHovered();

        // 如果使用游戏相机，直接跳过 Gizmo 和 Picking
        if (ctx.use_game_camera) {
            ImGui::End();
            ImGui::PopStyleVar();
            return;
        }

        // --- Gizmo 与 Picking 逻辑开始 ---
        bool gizmoOver = false;

        if (ctx.active_scene && ctx.selected != entt::null)
        {
            auto& reg = ctx.active_scene->getRegistry();
            if (reg.valid(ctx.selected) && reg.all_of<TransformComponent>(ctx.selected))
            {
                auto& tr = reg.get<TransformComponent>(ctx.selected);

                // 【关键修复 1】在这里定义 model，确保后续 Manipulate 能用到它
                glm::mat4 model = BuildModel(tr);

                // ImGuizmo 配置
                ImGuizmo::SetOrthographic(false);
                ImGuizmo::SetDrawlist(); // 使用当前窗口 Drawlist
                ImGuizmo::SetRect(canvasMin.x, canvasMin.y, canvasSize.x, canvasSize.y);

                // 【关键修复 2】确保传入的是 glm::mat4 的指针
                ImGuizmo::Manipulate(
                    glm::value_ptr(ctx.gizmo_view),
                    glm::value_ptr(ctx.gizmo_proj),
                    ImGuizmo::TRANSLATE,
                    ImGuizmo::LOCAL,
                    glm::value_ptr(model) // 这里的 model 必须在此作用域内已定义
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

        // Picking 逻辑
        if (ctx.viewport_hovered && ImGui::IsMouseClicked(0) && !ctx.gizmo_using && !gizmoOver)
        {
            ImVec2 mouse = ImGui::GetMousePos();
            int px = (int)(mouse.x - canvasMin.x);
            int py = (int)(canvasSize.y - 1 - (mouse.y - canvasMin.y)); // 直接计算翻转后的 Y

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
