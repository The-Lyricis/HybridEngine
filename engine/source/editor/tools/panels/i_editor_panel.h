#pragma once

#include <imgui.h>

namespace Hybrid
{
    struct EditorContext;

    enum class EditorPanelId
    {
        Hierarchy,
        Inspector,
        Project,
        SceneView,
        GameView,
    };

    struct EditorPanelState
    {
        EditorPanelId id = EditorPanelId::Hierarchy;
        const char* title = "";
        bool open = true;
    };

    enum class EditorDockSlot
    {
        Main,
        Right,
        LeftBottom,
        LeftTopLeft,
    };

    struct EditorPanelDescriptor
    {
        EditorPanelId id = EditorPanelId::Hierarchy;
        const char* title = "";
        bool default_open = true;
        bool show_in_window_menu = true;
        bool is_viewport = false;
        EditorDockSlot default_dock_slot = EditorDockSlot::Main;
    };

    class IEditorPanel
    {
    public:
        IEditorPanel(EditorPanelId id, const char* title, bool default_open = true)
            : m_state{ id, title, default_open }
        {
        }

        virtual ~IEditorPanel() = default;

        EditorPanelId getId() const { return m_state.id; }
        const char* getName() const { return m_state.title; }
        const EditorPanelState& getState() const { return m_state; }
        virtual void onImGuiRender(EditorContext& ctx) = 0;

        bool isOpen() const { return m_state.open; }
        void setOpen(bool open) { m_state.open = open; }

    protected:
        virtual void drawWindowContextMenu(EditorContext& ctx) {}

        void drawWindowContextMenuIfRequested(
            EditorContext& ctx,
            const char* popup_id = nullptr,
            ImGuiPopupFlags flags = ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)
        {
            if (!ImGui::BeginPopupContextWindow(popup_id, flags))
                return;

            drawWindowContextMenu(ctx);
            ImGui::EndPopup();
        }

        EditorPanelState m_state;
    };
} // namespace Hybrid
