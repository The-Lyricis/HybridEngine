#include "scene_view_panel.h"

#include "editor/core/editor_context.h"
#include "editor/core/editor_drag_drop.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <ImGuizmo.h>

#include <glm/gtc/type_ptr.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>

#include <glad/gl.h>
#include <stb_image.h>

#include "runtime/core/base/macro.h"
#include "runtime/core/base/math_util.h"
#include "runtime/modules/scene/components.h"
#include "runtime/modules/scene/entity.h"
#include "runtime/modules/scene/scene.h"

namespace Hybrid
{
    namespace
    {
        constexpr const char *kSceneViewPanelLogTag = "[SceneViewPanel]";

        static GLuint LoadTextureRGBA8(const std::string &path)
        {
            int w = 0, h = 0, comp = 0;
            stbi_set_flip_vertically_on_load(0);
            unsigned char *data = stbi_load(path.c_str(), &w, &h, &comp, 4);
            if (!data || w <= 0 || h <= 0)
            {
                if (data)
                    stbi_image_free(data);
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
            GLuint select = 0;
            GLuint move = 0;
            GLuint rotate = 0;
            GLuint scale = 0;
            GLuint local = 0;
            GLuint world = 0;
            bool loaded = false;
        };

        struct ToolbarResult
        {
            float height = 0.0f;
            bool interacted = false;
        };

        static ToolIcons g_ToolIcons;

        static glm::mat4 BuildModel(const TransformComponent &tr)
        {
            return tr.WorldMatrix;
        }

        enum class ToolMode
        {
            Select,
            Move,
            Rotate,
            Scale
        };
        static bool s_ToolIconsLoadFailedLogged = false;

        ToolMode toToolMode(SceneToolMode mode)
        {
            switch (mode)
            {
            case SceneToolMode::Select:
                return ToolMode::Select;
            case SceneToolMode::Move:
                return ToolMode::Move;
            case SceneToolMode::Rotate:
                return ToolMode::Rotate;
            case SceneToolMode::Scale:
                return ToolMode::Scale;
            default:
                return ToolMode::Move;
            }
        }

        SceneToolMode toSceneToolMode(ToolMode mode)
        {
            switch (mode)
            {
            case ToolMode::Select:
                return SceneToolMode::Select;
            case ToolMode::Move:
                return SceneToolMode::Move;
            case ToolMode::Rotate:
                return SceneToolMode::Rotate;
            case ToolMode::Scale:
                return SceneToolMode::Scale;
            default:
                return SceneToolMode::Move;
            }
        }

        const char *toolModeName(ToolMode mode)
        {
            switch (mode)
            {
            case ToolMode::Select:
                return "select";
            case ToolMode::Move:
                return "move";
            case ToolMode::Rotate:
                return "rotate";
            case ToolMode::Scale:
                return "scale";
            default:
                return "unknown";
            }
        }

        const char* gizmoSpaceName(GizmoSpace space)
        {
            switch (space)
            {
            case GizmoSpace::Local:
                return "local";
            case GizmoSpace::World:
                return "world";
            default:
                return "unknown";
            }
        }

        static void PushActiveToolStyle(bool active)
        {
            if (!active)
                return;
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.45f, 0.95f, 0.90f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.45f, 0.95f, 1.00f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.20f, 0.40f, 0.90f, 1.00f));
        }

        static void PopActiveToolStyle(bool active)
        {
            if (active)
                ImGui::PopStyleColor(3);
        }

        static ToolbarResult DrawSceneToolbar(const EditorContext &ctx, float available_width)
        {
            ToolbarResult result{};
            ToolMode tool_mode = toToolMode(ctx.scene_tool_mode);

            // 更紧凑的尺寸（接近 Unity）
            const float pad_x = 6.0f;
            const float pad_y = 4.0f;
            const ImVec2 button_size(28.0f, 24.0f);
            const ImVec2 icon_size(18.0f, 18.0f);

            const float toolbar_height = button_size.y + pad_y * 2.0f;
            result.height = toolbar_height;

            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(pad_x, pad_y));
            ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 0.0f);
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.09f, 0.10f, 0.12f, 1.0f));

            ImGui::BeginChild(
                "##SceneToolbar",
                ImVec2(available_width, toolbar_height),
                ImGuiChildFlags_None,
                ImGuiWindowFlags_NoScrollbar |
                    ImGuiWindowFlags_NoScrollWithMouse |
                    ImGuiWindowFlags_NoMove |
                    ImGuiWindowFlags_NoSavedSettings);

            auto draw_icon_button = [&](GLuint tex, const char *tip, bool active, int id)
            {
                PushActiveToolStyle(active);
                ImGui::PushID(id);

                bool pressed = false;

                // 使用“普通 Button + DrawList 画图标”的方式，避免 ImageButton 版本差异
                if (ImGui::Button("##toolbtn", button_size))
                    pressed = true;

                const ImVec2 item_min = ImGui::GetItemRectMin();
                const ImVec2 item_max = ImGui::GetItemRectMax();
                const ImVec2 item_size(item_max.x - item_min.x, item_max.y - item_min.y);
                const ImVec2 icon_min(
                    item_min.x + (item_size.x - icon_size.x) * 0.5f,
                    item_min.y + (item_size.y - icon_size.y) * 0.5f);
                const ImVec2 icon_max(icon_min.x + icon_size.x, icon_min.y + icon_size.y);

                if (tex != 0)
                    ImGui::GetWindowDrawList()->AddImage((ImTextureID)(intptr_t)tex, icon_min, icon_max);

                // 不显示文字，但 tooltip 可以保留（你要完全无提示也可删掉这两行）
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
                    ImGui::SetTooltip("%s", tip);

                if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem))
                    result.interacted = true;

                ImGui::PopID();
                PopActiveToolStyle(active);
                return pressed;
            };

            auto draw_tool_button = [&](GLuint tex, const char *tip, ToolMode mode)
            {
                if (draw_icon_button(tex, tip, tool_mode == mode, static_cast<int>(mode)))
                {
                    const_cast<EditorContext&>(ctx).scene_tool_mode = toSceneToolMode(mode);
                    result.interacted = true;
                }
            };

            // 让按钮在同一行紧凑排列
            ImGui::SetCursorPosY(pad_y); // 轻微顶对齐，避免内部被文本高度影响

            draw_tool_button(g_ToolIcons.select, "Select (Q)", ToolMode::Select);
            ImGui::SameLine(0.0f, 4.0f);
            draw_tool_button(g_ToolIcons.move, "Move (W)", ToolMode::Move);
            ImGui::SameLine(0.0f, 4.0f);
            draw_tool_button(g_ToolIcons.rotate, "Rotate (E)", ToolMode::Rotate);
            ImGui::SameLine(0.0f, 4.0f);
            draw_tool_button(g_ToolIcons.scale, "Scale (R)", ToolMode::Scale);
            ImGui::SameLine(0.0f, 10.0f);

            if (draw_icon_button(g_ToolIcons.local, "Local", ctx.gizmo_space == GizmoSpace::Local, 1001))
            {
                const_cast<EditorContext&>(ctx).gizmo_space = GizmoSpace::Local;
                result.interacted = true;
                HBD_CORE_INFO("{} gizmo_space_changed space={}", kSceneViewPanelLogTag, gizmoSpaceName(GizmoSpace::Local));
            }

            ImGui::SameLine(0.0f, 4.0f);
            if (draw_icon_button(g_ToolIcons.world, "World", ctx.gizmo_space == GizmoSpace::World, 1002))
            {
                const_cast<EditorContext&>(ctx).gizmo_space = GizmoSpace::World;
                result.interacted = true;
                HBD_CORE_INFO("{} gizmo_space_changed space={}", kSceneViewPanelLogTag, gizmoSpaceName(GizmoSpace::World));
            }

            if (ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem))
                result.interacted = true;

            ImGui::EndChild();
            ImGui::PopStyleColor(1);
            ImGui::PopStyleVar(3);

            return result;
        }
    } // namespace

    void SceneViewPanel::updateViewportState(EditorContext &ctx)
        {
            if (!m_state.open)
            {
                ctx.scene_viewport_image_hovered = false;
                ctx.scene_viewport_hovered = false;
                ctx.scene_viewport_focused = false;
            }
        }

        const char* gizmoSpaceName(GizmoSpace space)
        {
            switch (space)
            {
            case GizmoSpace::Local:
                return "local";
            case GizmoSpace::World:
                return "world";
            default:
                return "unknown";
            }
        }

    void SceneViewPanel::onImGuiRender(EditorContext &ctx)
        {
            if (!m_state.open)
                return;

            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{0, 0});
            ImGui::Begin(getName(), &m_state.open);

            if (!g_ToolIcons.loaded)
            {
                const std::string base = std::string(HYBRID_EDITOR_RESOURCES_DIR) + "/icons/";
                g_ToolIcons.select = LoadTextureRGBA8(base + "icon_editorTools_select.png");
                g_ToolIcons.move = LoadTextureRGBA8(base + "icon_editorTools_drag.png");
                g_ToolIcons.rotate = LoadTextureRGBA8(base + "icon_editorTools_rotate.png");
                g_ToolIcons.scale = LoadTextureRGBA8(base + "icon_editorTools_scale.png");
                g_ToolIcons.local = LoadTextureRGBA8(base + "icon_editorTools_local.png");
                g_ToolIcons.world = LoadTextureRGBA8(base + "icon_editorTools_world.png");
                g_ToolIcons.loaded = (g_ToolIcons.select && g_ToolIcons.move && g_ToolIcons.rotate &&
                                      g_ToolIcons.scale && g_ToolIcons.local && g_ToolIcons.world);
                if (!g_ToolIcons.loaded && !s_ToolIconsLoadFailedLogged)
                {
                    HBD_CORE_WARN("{} toolbar_icon_load_failed", kSceneViewPanelLogTag);
                    s_ToolIconsLoadFailedLogged = true;
                }
            }

            if (m_colorTextureID == 0)
            {
                if (!m_missingTextureLogged)
                {
                    HBD_CORE_WARN("{} viewport_texture_missing", kSceneViewPanelLogTag);
                    m_missingTextureLogged = true;
                }
            }
            else if (m_missingTextureLogged)
            {
                HBD_CORE_INFO("{} viewport_texture_ready texture_id={}", kSceneViewPanelLogTag, m_colorTextureID);
                m_missingTextureLogged = false;
            }

            const ToolbarResult toolbar = DrawSceneToolbar(ctx, ImGui::GetContentRegionAvail().x);
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() - ImGui::GetStyle().ItemSpacing.y);

            const ImVec2 canvas_min = ImGui::GetCursorScreenPos();
            ImVec2 canvas_size = ImGui::GetContentRegionAvail();
            if (canvas_size.x < 1.0f)
                canvas_size.x = 1.0f;
            if (canvas_size.y < 1.0f)
                canvas_size.y = 1.0f;

            ImGui::SetCursorScreenPos(canvas_min);
            ImGui::Image((ImTextureID)(intptr_t)m_colorTextureID, canvas_size, ImVec2(0, 1), ImVec2(1, 0));

            const ImVec2 viewport_min = ImGui::GetItemRectMin();
            const ImVec2 viewport_max = ImGui::GetItemRectMax();
            const bool viewport_hovered = ImGui::IsItemHovered();

            if (ImGui::BeginDragDropTarget())
            {
                const ImVec2 drop_mouse_pos = ImGui::GetMousePos();
                AssetID dropped_asset{};
                if (ctx.instantiate_scene_asset && EditorDragDrop::AcceptAsset(dropped_asset))
                {
                    HBD_CORE_INFO("{} asset_drop_requested asset_id={} x={} y={}",
                                  kSceneViewPanelLogTag,
                                  dropped_asset.value,
                                  drop_mouse_pos.x,
                                  drop_mouse_pos.y);
                    (void)ctx.instantiate_scene_asset(dropped_asset, drop_mouse_pos);
                }

                std::string dropped_rel_path;
                if (ctx.instantiate_scene_project_path && EditorDragDrop::AcceptProjectPath(dropped_rel_path))
                {
                    HBD_CORE_INFO("{} project_path_drop_requested path={} x={} y={}",
                                  kSceneViewPanelLogTag,
                                  dropped_rel_path,
                                  drop_mouse_pos.x,
                                  drop_mouse_pos.y);
                    (void)ctx.instantiate_scene_project_path(dropped_rel_path, drop_mouse_pos);
                }

                ImGui::EndDragDropTarget();
            }

            ctx.scene_viewport_image_hovered = viewport_hovered;
            ctx.scene_viewport_hovered = viewport_hovered;
            ctx.scene_viewport_focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
            ctx.scene_viewport_size = canvas_size;
            ctx.scene_viewport_min = viewport_min;
            ctx.scene_viewport_max = viewport_max;

            if (toolbar.interacted)
                ctx.scene_viewport_hovered = false;

            const ToolMode active_tool = toToolMode(ctx.scene_tool_mode);
            if (ctx.select_tool != (active_tool == ToolMode::Select))
            {
                HBD_CORE_INFO("{} tool_changed tool={}", kSceneViewPanelLogTag, toolModeName(active_tool));
            }
            ctx.select_tool = (active_tool == ToolMode::Select);

            bool gizmo_over = false;
            ctx.gizmo_using = false;

            const bool want_gizmo =
                (active_tool == ToolMode::Move || active_tool == ToolMode::Rotate || active_tool == ToolMode::Scale);

            if (want_gizmo && ctx.active_scene && ctx.activeEntity() != entt::null)
            {
                auto &reg = ctx.active_scene->getRegistry();
                if (reg.valid(ctx.activeEntity()) && reg.all_of<TransformComponent>(ctx.activeEntity()))
                {
                    auto &tr = reg.get<TransformComponent>(ctx.activeEntity());
                    const TransformSnapshot before_snapshot = CaptureTransformSnapshot(tr);
                    glm::mat4 model = BuildModel(tr);

                    ImGuizmo::SetOrthographic(false);
                    ImGuizmo::SetDrawlist();
                    ImGuizmo::SetRect(viewport_min.x, viewport_min.y, canvas_size.x, canvas_size.y);

                    ImGuizmo::OPERATION op = ImGuizmo::TRANSLATE;
                    if (active_tool == ToolMode::Rotate)
                        op = ImGuizmo::ROTATE;
                    if (active_tool == ToolMode::Scale)
                        op = ImGuizmo::SCALE;
                    ImGuizmo::MODE gizmo_mode = ImGuizmo::LOCAL;
                    if (op != ImGuizmo::SCALE)
                        gizmo_mode = (ctx.gizmo_space == GizmoSpace::World) ? ImGuizmo::WORLD : ImGuizmo::LOCAL;

                    bool snapping = ImGui::GetIO().KeyCtrl;
                    float snap[3] = {0.5f, 0.5f, 0.5f};
                    if (op == ImGuizmo::ROTATE)
                    {
                        snap[0] = 15.0f;
                        snap[1] = 15.0f;
                        snap[2] = 15.0f;
                    }
                    if (op == ImGuizmo::SCALE)
                    {
                        snap[0] = 0.1f;
                        snap[1] = 0.1f;
                        snap[2] = 0.1f;
                    }

                    ImGuizmo::Manipulate(
                        glm::value_ptr(ctx.gizmo_view),
                        glm::value_ptr(ctx.gizmo_proj),
                        op,
                        gizmo_mode,
                        glm::value_ptr(model),
                        nullptr,
                        snapping ? snap : nullptr);

                    ctx.gizmo_using = ImGuizmo::IsUsing();
                    gizmo_over = ImGuizmo::IsOver();

                    if (ctx.gizmo_using && !m_gizmo_drag_active)
                    {
                        m_gizmo_drag_active = true;
                        m_gizmo_drag_entity = ctx.activeEntity();
                        m_gizmo_drag_before = before_snapshot;
                    }

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
                            const auto &parent_tr = reg.get<TransformComponent>(tr.Parent);
                            local = glm::inverse(parent_tr.WorldMatrix) * model;
                        }

                        if (glm::decompose(local, scale, rotation, translation, skew, perspective))
                        {
                            if (op == ImGuizmo::TRANSLATE)
                                tr.Position = translation;
                            else if (op == ImGuizmo::ROTATE)
                                tr.Rotation = MathUtil::normalizeQuat(rotation);
                            else if (op == ImGuizmo::SCALE)
                                tr.Scale = scale;

                            tr.DirtyLocal = true;
                            ctx.active_scene->MarkDirtyRecursive(Entity(ctx.activeEntity(), &reg, ctx.active_scene));
                            ctx.markSceneDirty();
                        }
                    }
                }
            }

            if (m_gizmo_drag_active && !ctx.gizmo_using)
            {
                bool committed = false;
                if (ctx.active_scene &&
                    m_gizmo_drag_entity != entt::null)
                {
                    auto& reg = ctx.active_scene->getRegistry();
                    if (reg.valid(m_gizmo_drag_entity) && reg.all_of<TransformComponent>(m_gizmo_drag_entity))
                    {
                        const TransformSnapshot after_snapshot =
                            CaptureTransformSnapshot(reg.get<TransformComponent>(m_gizmo_drag_entity));
                        if (ctx.commit_transform_command)
                        {
                            ctx.commit_transform_command(m_gizmo_drag_entity, m_gizmo_drag_before, after_snapshot);
                            committed = true;
                        }
                    }
                }

                if (!committed)
                {
                    m_gizmo_drag_entity = entt::null;
                    m_gizmo_drag_before = {};
                }

                m_gizmo_drag_active = false;
                m_gizmo_drag_entity = entt::null;
                m_gizmo_drag_before = {};
            }

            if (ctx.scene_viewport_image_hovered &&
                ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
                !toolbar.interacted &&
                !ctx.gizmo_using &&
                !gizmo_over)
            {
                const ImVec2 mouse = ImGui::GetMousePos();
                const int px = (int)(mouse.x - viewport_min.x);
                const int py = (int)(canvas_size.y - 1 - (mouse.y - viewport_min.y));

                if (px >= 0 && py >= 0 && px < (int)canvas_size.x && py < (int)canvas_size.y)
                {
                    ctx.request_pick = true;
                    ctx.pick_x = px;
                    ctx.pick_y = py;
                    ctx.pick_toggle = ImGui::GetIO().KeyCtrl;
                    HBD_CORE_DEBUG("{} pick_requested x={} y={} toggle={}",
                                   kSceneViewPanelLogTag,
                                   px,
                                   py,
                                   ctx.pick_toggle);
                }
            }

            ImGui::End();
            ImGui::PopStyleVar();
        }
    } // namespace Hybrid
