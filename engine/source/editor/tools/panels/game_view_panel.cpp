#include "game_view_panel.h"

#include "editor/core/context/editor_context.h"
#include "runtime/core/base/macro.h"
#include "runtime/modules/project/project_context.h"
#include "runtime/modules/render/runtime/render_system.h"

#include <glad/gl.h>
#include <imgui.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iterator>

namespace Hybrid
{
    namespace
    {
        constexpr const char* kGameViewPanelLogTag = "[GameViewPanel]";
        constexpr const char* kModeNames[] = {"Free", "16:9", "1280 x 720", "1920 x 1080", "Custom"};

        ImVec2 fitAspect(ImVec2 available, float aspect)
        {
            available.x = std::max(1.0f, available.x);
            available.y = std::max(1.0f, available.y);
            ImVec2 result{available.x, available.x / aspect};
            if (result.y > available.y)
                result = {available.y * aspect, available.y};
            return result;
        }

        std::filesystem::path settingsPath()
        {
            return ProjectService::Get().settings / "EditorState.json";
        }
    }

    void GameViewPanel::loadSettings(EditorContext& ctx)
    {
        m_loaded_project = ProjectService::Get().project_file;
        std::ifstream input(settingsPath());
        if (!input)
            return;
        try
        {
            nlohmann::json root;
            input >> root;
            const int format_version = root.value("format_version", root.value("version", 0));
            if (format_version != 1 || !root.contains("game_view"))
                return;
            const auto& game = root["game_view"];
            const int mode = game.value("mode", 0);
            if (mode >= 0 && mode <= static_cast<int>(GameViewResolutionMode::Custom))
                ctx.game_view_settings.mode = static_cast<GameViewResolutionMode>(mode);
            ctx.game_view_settings.custom_width = game.value("custom_width", 1280);
            ctx.game_view_settings.custom_height = game.value("custom_height", 720);
        }
        catch (const std::exception& error)
        {
            HBD_CORE_WARN("{} settings_load_failed path={} reason={}", kGameViewPanelLogTag,
                          settingsPath().generic_string(), error.what());
        }
    }

    void GameViewPanel::saveSettings(const EditorContext& ctx)
    {
        const auto path = settingsPath();
        std::error_code error;
        std::filesystem::create_directories(path.parent_path(), error);
        nlohmann::json root = nlohmann::json::object();
        {
            std::ifstream input(path);
            if (input)
            {
                try { input >> root; }
                catch (const std::exception&) { root = nlohmann::json::object(); }
            }
        }
        root.erase("version");
        root["format_version"] = 1;
        root["game_view"] = {
            {"mode", static_cast<int>(ctx.game_view_settings.mode)},
            {"custom_width", ctx.game_view_settings.custom_width},
            {"custom_height", ctx.game_view_settings.custom_height},
        };
        const auto temporary = path.string() + ".tmp";
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        output << root.dump(2) << '\n';
        output.close();
        if (!output)
            return;
        std::filesystem::remove(path, error);
        error.clear();
        std::filesystem::rename(temporary, path, error);
        if (error)
            HBD_CORE_WARN("{} settings_save_failed path={} reason={}", kGameViewPanelLogTag,
                          path.generic_string(), error.message());
    }

    void GameViewPanel::updateViewportState(EditorContext& ctx)
    {
        if (!m_state.open)
        {
            ctx.game_viewport.image_hovered = false;
            ctx.game_viewport.hovered = false;
            ctx.game_viewport.focused = false;
        }
    }

    void GameViewPanel::onImGuiRender(EditorContext& ctx)
    {
        if (!m_state.open)
            return;
        if (m_loaded_project != ProjectService::Get().project_file)
            loadSettings(ctx);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{0, 0});
        ImGui::Begin(getName(), &m_state.open);
        ImGui::PopStyleVar();

        bool settings_changed = false;
        int mode = static_cast<int>(ctx.game_view_settings.mode);
        ImGui::SetNextItemWidth(150.0f);
        if (ImGui::Combo("##GameResolution", &mode, kModeNames, static_cast<int>(std::size(kModeNames))))
        {
            ctx.game_view_settings.mode = static_cast<GameViewResolutionMode>(mode);
            settings_changed = true;
        }

        GLint gl_limit = 8192;
        glGetIntegerv(GL_MAX_TEXTURE_SIZE, &gl_limit);
        const int dimension_limit = std::max(16, std::min(8192, gl_limit));
        if (ctx.game_view_settings.mode == GameViewResolutionMode::Custom)
        {
            ImGui::SameLine(); ImGui::SetNextItemWidth(90.0f);
            settings_changed |= ImGui::InputInt("W", &ctx.game_view_settings.custom_width, 0, 0);
            ImGui::SameLine(); ImGui::SetNextItemWidth(90.0f);
            settings_changed |= ImGui::InputInt("H", &ctx.game_view_settings.custom_height, 0, 0);
            ctx.game_view_settings.custom_width = std::clamp(ctx.game_view_settings.custom_width, 16, dimension_limit);
            ctx.game_view_settings.custom_height = std::clamp(ctx.game_view_settings.custom_height, 16, dimension_limit);
        }
        if (settings_changed)
            saveSettings(ctx);

        if (m_colorTextureID == 0 && !m_missingTextureLogged)
        {
            HBD_CORE_WARN("{} viewport_texture_missing", kGameViewPanelLogTag);
            m_missingTextureLogged = true;
        }
        else if (m_colorTextureID != 0 && m_missingTextureLogged)
        {
            HBD_CORE_INFO("{} viewport_texture_ready texture_id={}", kGameViewPanelLogTag, m_colorTextureID);
            m_missingTextureLogged = false;
        }

        const ImVec2 canvas_min = ImGui::GetCursorScreenPos();
        ImVec2 canvas_size = ImGui::GetContentRegionAvail();
        canvas_size.x = std::max(1.0f, canvas_size.x);
        canvas_size.y = std::max(1.0f, canvas_size.y);

        ImVec2 render_size = canvas_size;
        ImVec2 image_size = canvas_size;
        switch (ctx.game_view_settings.mode)
        {
        case GameViewResolutionMode::Aspect16x9:
            image_size = fitAspect(canvas_size, 16.0f / 9.0f);
            render_size = image_size;
            break;
        case GameViewResolutionMode::HD720:
            render_size = {1280.0f, 720.0f};
            image_size = fitAspect(canvas_size, 16.0f / 9.0f);
            break;
        case GameViewResolutionMode::FullHD:
            render_size = {1920.0f, 1080.0f};
            image_size = fitAspect(canvas_size, 16.0f / 9.0f);
            break;
        case GameViewResolutionMode::Custom:
            render_size = {static_cast<float>(ctx.game_view_settings.custom_width),
                           static_cast<float>(ctx.game_view_settings.custom_height)};
            image_size = fitAspect(canvas_size, render_size.x / render_size.y);
            break;
        default:
            break;
        }

        const ImVec2 image_min{canvas_min.x + (canvas_size.x - image_size.x) * 0.5f,
                               canvas_min.y + (canvas_size.y - image_size.y) * 0.5f};
        ImGui::GetWindowDrawList()->AddRectFilled(canvas_min,
            ImVec2(canvas_min.x + canvas_size.x, canvas_min.y + canvas_size.y), IM_COL32(0, 0, 0, 255));
        ImGui::SetCursorScreenPos(image_min);
        ImGui::Image(static_cast<ImTextureID>(m_colorTextureID),
                     image_size, ImVec2(0, 1), ImVec2(1, 0));

        const ImVec2 viewport_min = ImGui::GetItemRectMin();
        const ImVec2 viewport_max = ImGui::GetItemRectMax();
        const bool viewport_hovered = ImGui::IsItemHovered();
        if (ctx.debug.render_stats)
        {
            char line0[96]{};
            char line1[96]{};
            char line2[96]{};
            std::snprintf(line0, sizeof(line0), "%.1f FPS | %.2f ms | Render %.2f ms",
                          ctx.debug.render_stats->fps, ctx.debug.render_stats->frame_time_ms,
                          ctx.debug.render_stats->render_cpu_time_ms);
            std::snprintf(line1, sizeof(line1), "Draws %u | Triangles %u | Entities %u",
                          ctx.debug.render_stats->submitted_draw_calls,
                          ctx.debug.render_stats->submitted_triangles,
                          ctx.debug.render_stats->submitted_entities);
            std::snprintf(line2, sizeof(line2), "Culled %u / %u | Lights %u",
                          ctx.debug.render_stats->culled_items,
                          ctx.debug.render_stats->tested_items,
                          ctx.debug.render_stats->point_lights);
            const char* lines[] = {line0, line1, line2};
            float width = 0.0f;
            for (const char* line : lines)
                width = std::max(width, ImGui::CalcTextSize(line).x);
            const ImVec2 box_min(viewport_max.x - width - 24.0f, viewport_min.y + 8.0f);
            const ImVec2 box_max(viewport_max.x - 8.0f,
                                 box_min.y + ImGui::GetTextLineHeight() * 3.0f + 12.0f);
            ImDrawList* draw_list = ImGui::GetWindowDrawList();
            draw_list->AddRectFilled(box_min, box_max, IM_COL32(18, 22, 28, 190), 6.0f);
            for (int index = 0; index < 3; ++index)
                draw_list->AddText(ImVec2(box_min.x + 8.0f,
                                         box_min.y + 6.0f + index * ImGui::GetTextLineHeight()),
                                   ImGui::GetColorU32(ImGuiCol_Text), lines[index]);
        }
        ctx.game_viewport.image_hovered = viewport_hovered;
        ctx.game_viewport.hovered = viewport_hovered;
        ctx.game_viewport.focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
        ctx.game_viewport.size = image_size;
        ctx.game_viewport.render_size = render_size;
        ctx.game_viewport.min = viewport_min;
        ctx.game_viewport.max = viewport_max;

        ImGui::End();
    }
}
