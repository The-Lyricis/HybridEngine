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

#include <stb_image.h>
#include <glad/gl.h>

namespace Hybrid
{
    // -----------------------------
    // OpenGL texture loader (RGBA8)
    // -----------------------------
    static GLuint LoadTextureRGBA8(const std::string& path)
    {
        int w = 0, h = 0, comp = 0;
        stbi_set_flip_vertically_on_load(0);
        unsigned char* data = stbi_load(path.c_str(), &w, &h, &comp, 4);
        if (!data || w <= 0 || h <= 0)
        {
            if (data) stbi_image_free(data);
            return 0;
        }

        GLuint tex = 0;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
        glBindTexture(GL_TEXTURE_2D, 0);

        stbi_image_free(data);
        return tex;
    }

    struct ToolIcons
    {
        GLuint hand = 0;
        GLuint move = 0;
        GLuint rotate = 0;
        GLuint scale = 0;
        bool loaded = false;

        void destroy()
        {
            auto del = [](GLuint& t)
                {
                    if (t)
                    {
                        glDeleteTextures(1, &t);
                        t = 0;
                    }
                };
            del(hand); del(move); del(rotate); del(scale);
            loaded = false;
        }
    };

    static ToolIcons g_ToolIcons;

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

    static void PushActiveToolStyle(bool active)
    {
        if (!active) return;
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.45f, 0.95f, 0.90f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.45f, 0.95f, 1.00f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.20f, 0.40f, 0.90f, 1.00f));
    }

    static void PopActiveToolStyle(bool active)
    {
        if (!active) return;
        ImGui::PopStyleColor(3);
    }

    // 返回：本帧是否与工具栏发生交互（用于屏蔽 picking）
    static bool DrawUnityToolBarPNG(const ImVec2& canvasMin)
    {
        bool interacted = false;

        // Unity 快捷键：Q/W/E/R
        if (ImGui::IsKeyPressed(ImGuiKey_Q)) { s_Tool = ToolMode::Hand;   interacted = true; }
        if (ImGui::IsKeyPressed(ImGuiKey_W)) { s_Tool = ToolMode::Move;   interacted = true; }
        if (ImGui::IsKeyPressed(ImGuiKey_E)) { s_Tool = ToolMode::Rotate; interacted = true; }
        if (ImGui::IsKeyPressed(ImGuiKey_R)) { s_Tool = ToolMode::Scale;  interacted = true; }

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

        auto ImageToolButton = [&](GLuint tex, const char* tip, ToolMode mode)
            {
                const bool active = (s_Tool == mode);
                PushActiveToolStyle(active);

                ImGui::PushID((int)mode);
                bool pressed = false;

                if (tex != 0)
                {
                    // 说明：ImageButton 在不同 ImGui 版本签名略有差异，你当前版本大概率支持这个最常见签名
                    ImVec2 uv0(0, 0), uv1(1, 1);
                    ImVec4 bg(0, 0, 0, 0);           // 背景透明（高亮由按钮颜色控制）
                    ImVec4 tint(1, 1, 1, 1);         // 原色显示
                    pressed = ImGui::ImageButton("##toolbtn", (ImTextureID)(intptr_t)tex, btnSize, uv0, uv1, bg, tint);
                }
                else
                {
                    // fallback
                    const char* name = (mode == ToolMode::Hand) ? "Hand" :
                        (mode == ToolMode::Move) ? "Move" :
                        (mode == ToolMode::Rotate) ? "Rot" : "Scl";
                    pressed = ImGui::Button(name, btnSize);
                }

                if (pressed)
                {
                    s_Tool = mode;
                    interacted = true;
                }

                if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
                    ImGui::SetTooltip("%s", tip);

                ImGui::PopID();
                PopActiveToolStyle(active);
            };

        ImageToolButton(g_ToolIcons.hand, "Hand (Q)\nPan / Navigate", ToolMode::Hand);
        ImageToolButton(g_ToolIcons.move, "Move (W)", ToolMode::Move);
        ImageToolButton(g_ToolIcons.rotate, "Rotate (E)", ToolMode::Rotate);
        ImageToolButton(g_ToolIcons.scale, "Scale (R)", ToolMode::Scale);


        // 如果鼠标在工具栏窗口上，也认为发生交互（防止点击空白也触发 picking）
        if (ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem))
            interacted = true;

        ImGui::End();
        return interacted;
    }

    void ViewportPanel::onImGuiRender(EditorContext& ctx)
    {
        if (!m_open) return;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0, 0 });
        ImGui::Begin("Viewport", &m_open);

        // 一次性加载工具栏图标
        if (!g_ToolIcons.loaded)
        {
            const std::string base = std::string(HYBRID_ROOT_DIR) + "/resource/";

            g_ToolIcons.hand = LoadTextureRGBA8(base + "icon_editorTools_pan.png");
            g_ToolIcons.move = LoadTextureRGBA8(base + "icon_editorTools_drag.png");
            g_ToolIcons.rotate = LoadTextureRGBA8(base + "icon_editorTools_rotate.png");
            g_ToolIcons.scale = LoadTextureRGBA8(base + "icon_editorTools_scale.png");

            g_ToolIcons.loaded = (g_ToolIcons.hand && g_ToolIcons.move && g_ToolIcons.rotate && g_ToolIcons.scale);
        }

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

        // Game camera：禁用 gizmo/picking
        if (ctx.use_game_camera)
        {
            ctx.gizmo_using = false;
            ctx.request_pick = false;
            ImGui::End();
            ImGui::PopStyleVar();
            return;
        }

        // --- Unity 风格工具栏（PNG） ---
        const bool toolbarInteracted = DrawUnityToolBarPNG(canvasMin);

        // Hand 模式：给 RenderSystem/Engine 用
        ctx.pan_tool = (s_Tool == ToolMode::Hand);

        // --- Gizmo ---
        bool gizmoOver = false;
        ctx.gizmo_using = false;

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

                bool snapping = ImGui::GetIO().KeyCtrl;
                float snap[3] = { 0.5f, 0.5f, 0.5f };
                if (op == ImGuizmo::ROTATE) { snap[0] = 15.0f; snap[1] = 15.0f; snap[2] = 15.0f; }
                if (op == ImGuizmo::SCALE) { snap[0] = 0.1f;  snap[1] = 0.1f;  snap[2] = 0.1f; }

                ImGuizmo::Manipulate(
                    glm::value_ptr(ctx.gizmo_view),
                    glm::value_ptr(ctx.gizmo_proj),
                    op,
                    ImGuizmo::LOCAL,
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

        // --- Picking：工具栏交互 / gizmo hit / gizmo using 时不触发 ---
        if (ctx.viewport_image_hovered &&
            ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
            !toolbarInteracted &&
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
