#pragma once

#include <imgui.h>

namespace Hybrid::EditorInput
{
    inline bool selectionToggleModifier(const ImGuiIO& io = ImGui::GetIO())
    {
#ifdef __APPLE__
        return io.KeySuper;
#else
        return io.KeyCtrl;
#endif
    }

    inline bool contextMenuGesture()
    {
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Right))
            return true;
#ifdef __APPLE__
        // Control-click is the standard secondary-click fallback on macOS.
        const ImGuiIO& io = ImGui::GetIO();
        return io.KeyCtrl && !io.KeySuper && ImGui::IsMouseClicked(ImGuiMouseButton_Left);
#else
        return false;
#endif
    }

    inline bool beginItemContextPopup(const char* popup_id)
    {
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup) && contextMenuGesture())
            ImGui::OpenPopup(popup_id);
        return ImGui::BeginPopup(popup_id);
    }

    inline bool beginWindowContextPopup(const char* popup_id, bool no_open_over_items = true)
    {
        if (ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup) &&
            (!no_open_over_items || !ImGui::IsAnyItemHovered()) &&
            contextMenuGesture())
        {
            ImGui::OpenPopup(popup_id);
        }
        return ImGui::BeginPopup(popup_id);
    }

#ifdef __APPLE__
    inline constexpr const char* kOpenProjectShortcut = "Cmd+Shift+O";
    inline constexpr const char* kNewSceneShortcut = "Cmd+N";
    inline constexpr const char* kOpenSceneShortcut = "Cmd+O";
    inline constexpr const char* kSaveShortcut = "Cmd+S";
    inline constexpr const char* kSaveAsShortcut = "Cmd+Shift+S";
    inline constexpr const char* kUndoShortcut = "Cmd+Z";
    inline constexpr const char* kRedoShortcut = "Cmd+Shift+Z";
#else
    inline constexpr const char* kOpenProjectShortcut = "Ctrl+Shift+O";
    inline constexpr const char* kNewSceneShortcut = "Ctrl+N";
    inline constexpr const char* kOpenSceneShortcut = "Ctrl+O";
    inline constexpr const char* kSaveShortcut = "Ctrl+S";
    inline constexpr const char* kSaveAsShortcut = "Ctrl+Shift+S";
    inline constexpr const char* kUndoShortcut = "Ctrl+Z";
    inline constexpr const char* kRedoShortcut = "Ctrl+Y";
#endif
}
