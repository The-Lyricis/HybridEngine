#include "editor_shortcuts.h"

#include <imgui.h>

#include "editor/core/editor_commands.h"
#include "editor/core/editor_context.h"

namespace Hybrid
{
    void ProcessEditorShortcuts(EditorContext& ctx)
    {
        const ImGuiIO& io = ImGui::GetIO();
        const bool any_popup_open = ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId);
        const bool keyboard_item_active = ImGui::IsAnyItemActive() && io.WantCaptureKeyboard;
        if (io.WantTextInput || keyboard_item_active || any_popup_open)
            return;

        const bool shortcut_modifier = io.KeyCtrl || io.KeySuper;
        if (shortcut_modifier)
        {
            if (ImGui::IsKeyPressed(ImGuiKey_Z, false))
            {
                if (io.KeyShift)
                {
                    if (ctx.redo)
                        (void)ctx.redo();
                }
                else
                {
                    if (ctx.undo)
                        (void)ctx.undo();
                }
                return;
            }

            if (ImGui::IsKeyPressed(ImGuiKey_Y, false))
            {
                if (ctx.redo)
                    (void)ctx.redo();
                return;
            }

            if (ctx.execute_command)
            {
                if (ImGui::IsKeyPressed(ImGuiKey_N, false))
                {
                    (void)ctx.execute_command(EditorCommandId::NewScene);
                    return;
                }

                if (ImGui::IsKeyPressed(ImGuiKey_O, false))
                {
                    if (io.KeyShift)
                        (void)ctx.execute_command(EditorCommandId::OpenProject);
                    else
                        (void)ctx.execute_command(EditorCommandId::OpenScene);
                    return;
                }

                if (ImGui::IsKeyPressed(ImGuiKey_S, false))
                {
                    if (io.KeyShift)
                        (void)ctx.execute_command(EditorCommandId::SaveSceneAs);
                    else
                        (void)ctx.execute_command(EditorCommandId::SaveScene);
                    return;
                }
            }
        }

        if (ctx.suppress_tool_shortcuts || ctx.gizmo_using || !ctx.scene_viewport_focused)
            return;

        if (ImGui::IsKeyPressed(ImGuiKey_Q, false))
            ctx.scene_tool_mode = SceneToolMode::Select;
        else if (ImGui::IsKeyPressed(ImGuiKey_W, false))
            ctx.scene_tool_mode = SceneToolMode::Move;
        else if (ImGui::IsKeyPressed(ImGuiKey_E, false))
            ctx.scene_tool_mode = SceneToolMode::Rotate;
        else if (ImGui::IsKeyPressed(ImGuiKey_R, false))
            ctx.scene_tool_mode = SceneToolMode::Scale;
    }
}
