#include "viewport_panel.h"
#include "editor/core/editor_context.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <ImGuizmo.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>

#include "runtime/modules/scene/scene.h"
#include "runtime/modules/scene/components.h"
#include "runtime/modules/scene/entity.h"
#include "runtime/core/base/math_util.h"

#include <stb_image.h>
#include <glad/gl.h>

namespace Hybrid
{
    namespace
    {
        bool& viewportHovered(EditorContext& ctx, ViewportPanelMode mode)
        {
            return mode == ViewportPanelMode::Scene ? ctx.scene_viewport_hovered : ctx.game_viewport_hovered;
        }

        bool& viewportFocused(EditorContext& ctx, ViewportPanelMode mode)
        {
            return mode == ViewportPanelMode::Scene ? ctx.scene_viewport_focused : ctx.game_viewport_focused;
        }

        bool& viewportImageHovered(EditorContext& ctx, ViewportPanelMode mode)
        {
            return mode == ViewportPanelMode::Scene ? ctx.scene_viewport_image_hovered : ctx.game_viewport_image_hovered;
        }

        ImVec2& viewportSize(EditorContext& ctx, ViewportPanelMode mode)
        {
            return mode == ViewportPanelMode::Scene ? ctx.scene_viewport_size : ctx.game_viewport_size;
        }

        ImVec2& viewportMin(EditorContext& ctx, ViewportPanelMode mode)
        {
            return mode == ViewportPanelMode::Scene ? ctx.scene_viewport_min : ctx.game_viewport_min;
        }

        ImVec2& viewportMax(EditorContext& ctx, ViewportPanelMode mode)
        {
            return mode == ViewportPanelMode::Scene ? ctx.scene_viewport_max : ctx.game_viewport_max;
        }
    } // namespace

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
        return tr.WorldMatrix;
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

    // Returns whether this frame interacted with the toolbar (used to block picking).
    static bool DrawUnityToolBarPNG(const EditorContext& ctx, const ImVec2& canvasMin)
    {
        bool interacted = false;

        // Unity-style shortcuts: Q / W / E / R
        if (!ctx.suppress_tool_shortcuts)
        {
            if (ImGui::IsKeyPressed(ImGuiKey_Q)) { s_Tool = ToolMode::Hand;   interacted = true; }
            if (ImGui::IsKeyPressed(ImGuiKey_W)) { s_Tool = ToolMode::Move;   interacted = true; }
            if (ImGui::IsKeyPressed(ImGuiKey_E)) { s_Tool = ToolMode::Rotate; interacted = true; }
            if (ImGui::IsKeyPressed(ImGuiKey_R)) { s_Tool = ToolMode::Scale;  interacted = true; }
        }

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

        const ImVec2 button_size(36.0f, 20.0f);
        const ImVec2 icon_size(18.0f, 18.0f);

        auto ImageToolButton = [&](GLuint tex, const char* tip, ToolMode mode)
            {
                const bool active = (s_Tool == mode);
                PushActiveToolStyle(active);

                ImGui::PushID((int)mode);
                bool pressed = false;

                if (tex != 0)
                {
                    const ImVec2 cursor = ImGui::GetCursorScreenPos();
                    if (ImGui::Button("##toolbtn", button_size))
                        pressed = true;

                    const ImVec2 icon_min(
                        cursor.x + (button_size.x - icon_size.x) * 0.5f,
                        cursor.y + (button_size.y - icon_size.y) * 0.5f);
                    const ImVec2 icon_max(icon_min.x + icon_size.x, icon_min.y + icon_size.y);
                    ImGui::GetWindowDrawList()->AddImage((ImTextureID)(intptr_t)tex, icon_min, icon_max);
                }
                else
                {
                    // fallback
                    const char* name = (mode == ToolMode::Hand) ? "Hand" :
                        (mode == ToolMode::Move) ? "Move" :
                        (mode == ToolMode::Rotate) ? "Rot" : "Scl";
                    pressed = ImGui::Button(name, button_size);
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


        // Treat hovering on toolbar as interaction to suppress viewport picking.
        if (ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem))
            interacted = true;

        ImGui::End();
        return interacted;
    }

    void ViewportPanel::updateViewportState(EditorContext& ctx)
    {
        if (!m_open)
        {
            viewportImageHovered(ctx, m_mode) = false;
            viewportHovered(ctx, m_mode) = false;
            viewportFocused(ctx, m_mode) = false;
            return;
        }

        const bool has_canvas_rect =
            (viewportMax(ctx, m_mode).x > viewportMin(ctx, m_mode).x) &&
            (viewportMax(ctx, m_mode).y > viewportMin(ctx, m_mode).y);
        const bool hovered = has_canvas_rect &&
            ImGui::IsMouseHoveringRect(viewportMin(ctx, m_mode), viewportMax(ctx, m_mode), false);
        viewportImageHovered(ctx, m_mode) = hovered;
        viewportHovered(ctx, m_mode) = hovered;

        if (hovered &&
            (ImGui::IsMouseClicked(ImGuiMouseButton_Left) ||
             ImGui::IsMouseClicked(ImGuiMouseButton_Middle) ||
             ImGui::IsMouseClicked(ImGuiMouseButton_Right)))
        {
            viewportFocused(ctx, m_mode) = true;
        }
        else if (!hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            viewportFocused(ctx, m_mode) = false;
        }
    }

    void ViewportPanel::onImGuiRender(EditorContext& ctx)
    {
        if (!m_open) return;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0, 0 });
        ImGui::Begin(getName(), &m_open);

        const bool is_scene_view = (m_mode == ViewportPanelMode::Scene);

        // Lazy-load toolbar icons once.
        if (!g_ToolIcons.loaded)
        {
            const std::string base = std::string(HYBRID_ROOT_DIR) + "/assets/icons/";

            g_ToolIcons.hand = LoadTextureRGBA8(base + "icon_editorTools_pan.png");
            g_ToolIcons.move = LoadTextureRGBA8(base + "icon_editorTools_drag.png");
            g_ToolIcons.rotate = LoadTextureRGBA8(base + "icon_editorTools_rotate.png");
            g_ToolIcons.scale = LoadTextureRGBA8(base + "icon_editorTools_scale.png");

            g_ToolIcons.loaded = (g_ToolIcons.hand && g_ToolIcons.move && g_ToolIcons.rotate && g_ToolIcons.scale);
        }

        // Camera mode controls.
        {
            ImGui::SetCursorPosX(5.0f);
            ImGui::TextUnformatted("Camera:");
            ImGui::SameLine();
            ImGui::TextUnformatted(ctx.use_game_camera ? "Game" : "Editor");
            ImGui::SameLine();
            if (ImGui::Button(ctx.use_game_camera ? "Switch to Editor Camera" : "Switch to Game Camera"))
                ctx.use_game_camera = !ctx.use_game_camera;
            ImGui::SameLine();
            ImGui::Checkbox("Show Colliders", &ctx.show_collider_debug);

            ImGui::Separator();
        }

        // Viewport canvas region.
        ImVec2 canvasMin = ImGui::GetCursorScreenPos();
        ImVec2 canvasSize = ImGui::GetContentRegionAvail();
        if (canvasSize.x < 1.0f) canvasSize.x = 1.0f;
        if (canvasSize.y < 1.0f) canvasSize.y = 1.0f;
        ImVec2 canvasMax = { canvasMin.x + canvasSize.x, canvasMin.y + canvasSize.y };

        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddImage((ImTextureID)(intptr_t)m_colorTextureID, canvasMin, canvasMax, { 0, 1 }, { 1, 0 });

        // Viewport canvas region.
        viewportImageHovered(ctx, m_mode) = ImGui::IsMouseHoveringRect(canvasMin, canvasMax, false);
        viewportHovered(ctx, m_mode) = viewportImageHovered(ctx, m_mode);
        viewportFocused(ctx, m_mode) = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

        viewportSize(ctx, m_mode) = canvasSize;
        viewportMin(ctx, m_mode) = canvasMin;
        viewportMax(ctx, m_mode) = canvasMax;

        // Game viewport is display-only for now.
        if (!is_scene_view)
        {
            ctx.gizmo_using = false;
            ctx.request_pick = false;
            ImGui::End();
            ImGui::PopStyleVar();
            return;
        }

        // Unity-style toolbar (PNG icons).
        const bool toolbarInteracted = DrawUnityToolBarPNG(ctx, canvasMin);

        // Hand mode flag consumed by runtime camera control path.
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
                ImGuizmo::SetDrawlist(); // Draw into current ImGui window draw list.
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
                    glm::vec3 skew{};
                    glm::vec4 perspective{};
                    glm::vec3 scale{};
                    glm::quat rotation{};
                    glm::vec3 translation{};
                    glm::mat4 local = model;
                    if (tr.Parent != entt::null && reg.valid(tr.Parent) && reg.all_of<TransformComponent>(tr.Parent))
                    {
                        const auto& parent_tr = reg.get<TransformComponent>(tr.Parent);
                        local = glm::inverse(parent_tr.WorldMatrix) * model;
                    }

                    if (glm::decompose(local, scale, rotation, translation, skew, perspective))
                    {
                        if (op == ImGuizmo::TRANSLATE)
                        {
                            tr.Position = translation;
                        }
                        else if (op == ImGuizmo::ROTATE)
                        {
                            tr.Rotation = MathUtil::normalizeQuat(rotation);
                        }
                        else if (op == ImGuizmo::SCALE)
                        {
                            tr.Scale = scale;
                        }

                        tr.DirtyLocal = true;
                        ctx.active_scene->MarkDirtyRecursive(Entity(ctx.selected, &reg, ctx.active_scene));
                        ctx.markSceneDirty();
                    }
                }
            }
        }

        // Picking: ignore clicks when toolbar/gizmo is interacting.
        if (ctx.scene_viewport_image_hovered &&
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
