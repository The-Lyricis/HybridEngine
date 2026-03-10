#pragma once

#include <imgui.h>

namespace Hybrid
{
    struct EditorContext;

    class IEditorPanel
    {
    public:
        virtual ~IEditorPanel() = default;

        virtual const char* getName() const = 0;
        virtual void onImGuiRender(EditorContext& ctx) = 0;

        bool isOpen() const { return m_open; }
        void setOpen(bool open) { m_open = open; }

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

        bool m_open = true;
    };
} // namespace Hybrid
