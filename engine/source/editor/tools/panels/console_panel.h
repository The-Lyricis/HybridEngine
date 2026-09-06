#pragma once

#include "i_editor_panel.h"

#include <array>
#include <string>
#include <vector>

#include "runtime/core/log/log_system.h"

namespace Hybrid
{
    class ConsolePanel final : public IEditorPanel
    {
    public:
        ConsolePanel() : IEditorPanel(EditorPanelId::Console, "Console") {}
        void onImGuiRender(EditorContext& ctx) override;

    private:
        bool isVisible(const LogEntry& entry) const;
        LogBufferSnapshot m_paused_snapshot;
        std::array<char, 256> m_search{};
        bool m_show_trace = false;
        bool m_show_debug = true;
        bool m_show_info = true;
        bool m_show_warning = true;
        bool m_show_error = true;
        bool m_show_core = true;
        bool m_show_client = true;
        bool m_paused = false;
        bool m_auto_scroll = true;
    };
}
