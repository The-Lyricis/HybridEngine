#include "editor_shortcuts.h"

#include <imgui.h>

#include "editor/core/commands/editor_commands.h"
#include "editor/core/context/editor_context.h"

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
                    if (ctx.commands.redo)
                        (void)ctx.commands.redo();
                }
                else
                {
                    if (ctx.commands.undo)
                        (void)ctx.commands.undo();
                }
                return;
            }

            if (ImGui::IsKeyPressed(ImGuiKey_Y, false))
            {
                if (ctx.commands.redo)
                    (void)ctx.commands.redo();
                return;
            }

            if (ctx.commands.execute_command)
            {
                if (ImGui::IsKeyPressed(ImGuiKey_N, false))
                {
                    (void)ctx.commands.execute_command(EditorCommandId::NewScene);
                    return;
                }

                if (ImGui::IsKeyPressed(ImGuiKey_O, false))
                {
                    if (io.KeyShift)
                        (void)ctx.commands.execute_command(EditorCommandId::OpenProject);
                    else
                        (void)ctx.commands.execute_command(EditorCommandId::OpenScene);
                    return;
                }

                if (ImGui::IsKeyPressed(ImGuiKey_S, false))
                {
                    if (io.KeyShift)
                        (void)ctx.commands.execute_command(EditorCommandId::SaveSceneAs);
                    else
                        (void)ctx.commands.execute_command(EditorCommandId::SaveScene);
                    return;
                }
            }
        }

        if (ctx.gizmo.suppress_tool_shortcuts || ctx.gizmo.using_gizmo || !ctx.scene_viewport.focused)
            return;

        if (ImGui::IsKeyPressed(ImGuiKey_Q, false))
            ctx.gizmo.tool_mode = SceneToolMode::Select;
        else if (ImGui::IsKeyPressed(ImGuiKey_W, false))
            ctx.gizmo.tool_mode = SceneToolMode::Move;
        else if (ImGui::IsKeyPressed(ImGuiKey_E, false))
            ctx.gizmo.tool_mode = SceneToolMode::Rotate;
        else if (ImGui::IsKeyPressed(ImGuiKey_R, false))
            ctx.gizmo.tool_mode = SceneToolMode::Scale;
    }
}
