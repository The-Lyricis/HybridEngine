#include "scene_view_toolbar.h"

#include <string>

#include <glad/gl.h>
#include <imgui.h>
#include <stb_image.h>

#include "runtime/core/base/macro.h"

namespace Hybrid
{
    namespace
    {
        constexpr const char* kSceneViewToolbarLogTag = "[SceneViewToolbar]";

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

        ToolIcons g_ToolIcons;
        bool g_ToolIconsLoadFailedLogged = false;

        static GLuint LoadTextureRGBA8(const std::string& path)
        {
            int w = 0;
            int h = 0;
            int comp = 0;
            stbi_set_flip_vertically_on_load(0);
            unsigned char* data = stbi_load(path.c_str(), &w, &h, &comp, 4);
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

        void ensureToolIconsLoaded()
        {
            if (g_ToolIcons.loaded)
                return;

            const std::string base = std::string(HYBRID_EDITOR_RESOURCES_DIR) + "/icons/";
            g_ToolIcons.select = LoadTextureRGBA8(base + "icon_editorTools_select.png");
            g_ToolIcons.move = LoadTextureRGBA8(base + "icon_editorTools_drag.png");
            g_ToolIcons.rotate = LoadTextureRGBA8(base + "icon_editorTools_rotate.png");
            g_ToolIcons.scale = LoadTextureRGBA8(base + "icon_editorTools_scale.png");
            g_ToolIcons.local = LoadTextureRGBA8(base + "icon_editorTools_local.png");
            g_ToolIcons.world = LoadTextureRGBA8(base + "icon_editorTools_world.png");
            g_ToolIcons.loaded = (g_ToolIcons.select && g_ToolIcons.move && g_ToolIcons.rotate &&
                                  g_ToolIcons.scale && g_ToolIcons.local && g_ToolIcons.world);

            if (!g_ToolIcons.loaded && !g_ToolIconsLoadFailedLogged)
            {
                HBD_CORE_WARN("{} toolbar_icon_load_failed", kSceneViewToolbarLogTag);
                g_ToolIconsLoadFailedLogged = true;
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
    } // namespace

    SceneViewToolbarResult DrawSceneViewToolbar(EditorContext& ctx, float available_width)
    {
        ensureToolIconsLoaded();

        SceneViewToolbarResult result{};
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

        auto draw_icon_button = [&](GLuint tex, const char* tip, bool active, int id)
        {
            PushActiveToolStyle(active);
            ImGui::PushID(id);

            bool pressed = false;
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

            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
                ImGui::SetTooltip("%s", tip);

            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem))
                result.interacted = true;

            ImGui::PopID();
            PopActiveToolStyle(active);
            return pressed;
        };

        auto draw_tool_button = [&](GLuint tex, const char* tip, SceneToolMode mode, int id)
        {
            if (draw_icon_button(tex, tip, ctx.gizmo.tool_mode == mode, id))
            {
                ctx.gizmo.tool_mode = mode;
                result.interacted = true;
            }
        };

        ImGui::SetCursorPosY(pad_y);

        draw_tool_button(g_ToolIcons.select, "Select (Q)", SceneToolMode::Select, 0);
        ImGui::SameLine(0.0f, 4.0f);
        draw_tool_button(g_ToolIcons.move, "Move (W)", SceneToolMode::Move, 1);
        ImGui::SameLine(0.0f, 4.0f);
        draw_tool_button(g_ToolIcons.rotate, "Rotate (E)", SceneToolMode::Rotate, 2);
        ImGui::SameLine(0.0f, 4.0f);
        draw_tool_button(g_ToolIcons.scale, "Scale (R)", SceneToolMode::Scale, 3);
        ImGui::SameLine(0.0f, 10.0f);

        if (draw_icon_button(g_ToolIcons.local, "Local", ctx.gizmo.space == GizmoSpace::Local, 1001))
        {
            ctx.gizmo.space = GizmoSpace::Local;
            result.interacted = true;
            HBD_CORE_INFO("{} gizmo_space_changed space={}", kSceneViewToolbarLogTag, gizmoSpaceName(GizmoSpace::Local));
        }

        ImGui::SameLine(0.0f, 4.0f);
        if (draw_icon_button(g_ToolIcons.world, "World", ctx.gizmo.space == GizmoSpace::World, 1002))
        {
            ctx.gizmo.space = GizmoSpace::World;
            result.interacted = true;
            HBD_CORE_INFO("{} gizmo_space_changed space={}", kSceneViewToolbarLogTag, gizmoSpaceName(GizmoSpace::World));
        }

        if (ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem))
            result.interacted = true;

        ImGui::EndChild();
        ImGui::PopStyleColor(1);
        ImGui::PopStyleVar(3);
        return result;
    }
} // namespace Hybrid
