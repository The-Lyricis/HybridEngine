#include "console_panel.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstring>

#include "editor/core/context/editor_context.h"

namespace Hybrid
{
    bool ConsolePanel::isVisible(const LogEntry& entry) const
    {
        const bool logger_visible =
            (m_show_core && entry.logger == "HYBRID_CORE") ||
            (m_show_client && entry.logger == "HYBRID_CLIENT");
        if (!logger_visible)
            return false;

        bool level_visible = false;
        switch (entry.level)
        {
        case spdlog::level::trace: level_visible = m_show_trace; break;
        case spdlog::level::debug: level_visible = m_show_debug; break;
        case spdlog::level::info: level_visible = m_show_info; break;
        case spdlog::level::warn: level_visible = m_show_warning; break;
        default: level_visible = m_show_error; break;
        }
        if (!level_visible)
            return false;

        if (m_search[0] == '\0')
            return true;
        std::string haystack = entry.logger + " " + entry.message;
        std::string needle(m_search.data());
        std::transform(haystack.begin(), haystack.end(), haystack.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        std::transform(needle.begin(), needle.end(), needle.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return haystack.find(needle) != std::string::npos;
    }

    void ConsolePanel::onImGuiRender(EditorContext&)
    {
        if (!m_state.open)
            return;
        ImGui::Begin(getName(), &m_state.open);

        if (ImGui::Button("Clear"))
        {
            LogSystem::clearBufferedEntries();
            m_paused_snapshot = {};
        }
        ImGui::SameLine();
        if (ImGui::Checkbox("Pause", &m_paused) && m_paused)
            m_paused_snapshot = LogSystem::bufferedEntries();
        ImGui::SameLine(); ImGui::Checkbox("Auto-scroll", &m_auto_scroll);
        ImGui::SameLine(); ImGui::Checkbox("Core", &m_show_core);
        ImGui::SameLine(); ImGui::Checkbox("Client", &m_show_client);

        ImGui::Checkbox("Trace", &m_show_trace); ImGui::SameLine();
        ImGui::Checkbox("Debug", &m_show_debug); ImGui::SameLine();
        ImGui::Checkbox("Info", &m_show_info); ImGui::SameLine();
        ImGui::Checkbox("Warning", &m_show_warning); ImGui::SameLine();
        ImGui::Checkbox("Error", &m_show_error);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputTextWithHint("##ConsoleSearch", "Search logs...", m_search.data(), m_search.size());

        const LogBufferSnapshot snapshot = m_paused ? m_paused_snapshot : LogSystem::bufferedEntries();
        bool copy_requested = false;
        if (ImGui::BeginPopupContextWindow("##ConsoleContext", ImGuiPopupFlags_MouseButtonRight))
        {
            copy_requested = ImGui::MenuItem("Copy visible logs");
            ImGui::EndPopup();
        }

        std::string clipboard;
        ImGui::BeginChild("##ConsoleEntries", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
        for (const LogEntry& entry : snapshot.entries)
        {
            if (!isVisible(entry))
                continue;
            const auto time = std::chrono::system_clock::to_time_t(entry.timestamp);
            std::tm local{};
#ifdef _WIN32
            localtime_s(&local, &time);
#else
            localtime_r(&time, &local);
#endif
            char prefix[128]{};
            const auto level_view = spdlog::level::to_string_view(entry.level);
            const std::string level_name(level_view.data(), level_view.size());
            std::snprintf(prefix, sizeof(prefix), "[%02d:%02d:%02d] [%s] [tid=%llu] ",
                          local.tm_hour, local.tm_min, local.tm_sec,
                          level_name.c_str(),
                          static_cast<unsigned long long>(entry.thread_id));
            ImVec4 color = ImGui::GetStyleColorVec4(ImGuiCol_Text);
            if (entry.level >= spdlog::level::err) color = ImVec4(1.0f, 0.35f, 0.35f, 1.0f);
            else if (entry.level == spdlog::level::warn) color = ImVec4(1.0f, 0.78f, 0.25f, 1.0f);
            ImGui::PushStyleColor(ImGuiCol_Text, color);
            ImGui::TextUnformatted((std::string(prefix) + entry.logger + ": " + entry.message).c_str());
            ImGui::PopStyleColor();
            if (copy_requested)
                clipboard += std::string(prefix) + entry.logger + ": " + entry.message + "\n";
        }
        if (copy_requested)
            ImGui::SetClipboardText(clipboard.c_str());
        if (m_auto_scroll && !m_paused && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 4.0f)
            ImGui::SetScrollHereY(1.0f);
        ImGui::EndChild();
        ImGui::End();
    }
}
