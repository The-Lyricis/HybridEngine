#include "scene_view_toolbar.h"

#include <algorithm>
#include <filesystem>
#include <string>

#include <imgui.h>

#include "editor/services/render/editor_texture_service.h"
#include "runtime/core/base/macro.h"

namespace Hybrid
{
    namespace
    {
        constexpr const char* kSceneViewToolbarLogTag = "[SceneViewToolbar]";

        struct ToolIcons
        {
            EditorImageHandle select;
            EditorImageHandle move;
            EditorImageHandle rotate;
            EditorImageHandle scale;
            EditorImageHandle local;
            EditorImageHandle world;
        };

        bool g_ToolIconsLoadFailedLogged = false;

        ToolIcons loadToolIcons(EditorContext& ctx)
        {
            if (!ctx.textures)
                return {};
            const std::filesystem::path base =
                std::filesystem::path(HYBRID_EDITOR_RESOURCES_DIR) / "icons";
            ToolIcons icons{
                ctx.textures->loadRgba8(base / "icon_editorTools_select.png"),
                ctx.textures->loadRgba8(base / "icon_editorTools_drag.png"),
                ctx.textures->loadRgba8(base / "icon_editorTools_rotate.png"),
                ctx.textures->loadRgba8(base / "icon_editorTools_scale.png"),
                ctx.textures->loadRgba8(base / "icon_editorTools_local.png"),
                ctx.textures->loadRgba8(base / "icon_editorTools_world.png")};
            if ((!icons.select || !icons.move || !icons.rotate || !icons.scale ||
                 !icons.local || !icons.world) && !g_ToolIconsLoadFailedLogged)
            {
                HBD_CORE_WARN("{} toolbar_icon_load_failed", kSceneViewToolbarLogTag);
                g_ToolIconsLoadFailedLogged = true;
            }
            return icons;
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
        const ToolIcons icons = loadToolIcons(ctx);

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

        auto draw_icon_button = [&](EditorImageHandle image, const char* tip, bool active, int id)
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

            if (image && ctx.textures)
                ImGui::GetWindowDrawList()->AddImage(ctx.textures->imageId(image), icon_min, icon_max);

            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
                ImGui::SetTooltip("%s", tip);

            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem))
                result.interacted = true;

            ImGui::PopID();
            PopActiveToolStyle(active);
            return pressed;
        };

        auto draw_tool_button = [&](EditorImageHandle image, const char* tip, SceneToolMode mode, int id)
        {
            if (draw_icon_button(image, tip, ctx.gizmo.tool_mode == mode, id))
            {
                ctx.gizmo.tool_mode = mode;
                result.interacted = true;
            }
        };

        ImGui::SetCursorPosY(pad_y);

        draw_tool_button(icons.select, "Select (Q)", SceneToolMode::Select, 0);
        ImGui::SameLine(0.0f, 4.0f);
        draw_tool_button(icons.move, "Move (W)", SceneToolMode::Move, 1);
        ImGui::SameLine(0.0f, 4.0f);
        draw_tool_button(icons.rotate, "Rotate (E)", SceneToolMode::Rotate, 2);
        ImGui::SameLine(0.0f, 4.0f);
        draw_tool_button(icons.scale, "Scale (R)", SceneToolMode::Scale, 3);
        ImGui::SameLine(0.0f, 10.0f);

        if (draw_icon_button(icons.local, "Local", ctx.gizmo.space == GizmoSpace::Local, 1001))
        {
            ctx.gizmo.space = GizmoSpace::Local;
            result.interacted = true;
            HBD_CORE_INFO("{} gizmo_space_changed space={}", kSceneViewToolbarLogTag, gizmoSpaceName(GizmoSpace::Local));
        }

        ImGui::SameLine(0.0f, 4.0f);
        if (draw_icon_button(icons.world, "World", ctx.gizmo.space == GizmoSpace::World, 1002))
        {
            ctx.gizmo.space = GizmoSpace::World;
            result.interacted = true;
            HBD_CORE_INFO("{} gizmo_space_changed space={}", kSceneViewToolbarLogTag, gizmoSpaceName(GizmoSpace::World));
        }

        const float current_x = ImGui::GetCursorPosX();
        const float controls_width = available_width >= 620.0f ? 430.0f : 176.0f;
        if (available_width > current_x + controls_width + 18.0f)
        {
            ImGui::SameLine();
            ImGui::SetCursorPosX((std::max)(current_x + 16.0f, available_width - controls_width));

            if (ImGui::Checkbox("Post", &ctx.debug.enable_post_process))
                result.interacted = true;
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
                ImGui::SetTooltip("Post Process");

            ImGui::BeginDisabled(!ctx.debug.enable_post_process);
            ImGui::SameLine(0.0f, 8.0f);
            if (ImGui::Checkbox("TM", &ctx.debug.enable_tone_mapping))
                result.interacted = true;
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
                ImGui::SetTooltip("Tone Mapping");

            ImGui::SameLine(0.0f, 8.0f);
            if (ImGui::Checkbox("Gamma", &ctx.debug.enable_gamma_correction))
                result.interacted = true;
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
                ImGui::SetTooltip("Gamma Correction");

            if (available_width >= 620.0f)
            {
                ImGui::SameLine(0.0f, 8.0f);
                ImGui::SetNextItemWidth(90.0f);
                if (ImGui::SliderFloat("Exp", &ctx.debug.post_process_exposure, 0.0f, 5.0f, "%.2f"))
                    result.interacted = true;

                ImGui::SameLine(0.0f, 8.0f);
                ImGui::SetNextItemWidth(90.0f);
                if (ImGui::SliderFloat("##GammaValue", &ctx.debug.post_process_gamma, 0.1f, 4.0f, "%.2f"))
                    result.interacted = true;
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
                    ImGui::SetTooltip("Gamma Value");
            }
            ImGui::EndDisabled();
        }

        if (ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem))
            result.interacted = true;

        ImGui::EndChild();
        ImGui::PopStyleColor(1);
        ImGui::PopStyleVar(3);
        return result;
    }
} // namespace Hybrid
