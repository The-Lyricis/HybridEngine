#include "project_settings_panel.h"

#include <cstdio>

#include "editor/core/context/editor_context.h"
#include "runtime/modules/project/project_context.h"

namespace Hybrid
{
    void ProjectSettingsPanel::synchronize()
    {
        const ProjectContext& project = ProjectService::Get();
        m_project_file = project.project_file;
        std::snprintf(m_startup_scene.data(), m_startup_scene.size(), "%s", project.startup_scene.c_str());
        m_message.clear();
    }

    void ProjectSettingsPanel::onImGuiRender(EditorContext& ctx)
    {
        if (!m_state.open)
            return;
        const ProjectContext& project = ProjectService::Get();
        if (m_project_file != project.project_file)
            synchronize();

        ImGui::SetNextWindowSize(ImVec2(620.0f, 360.0f), ImGuiCond_FirstUseEver);
        ImGui::Begin(getName(), &m_state.open);
        ImGui::Text("Project: %s", project.project_file.stem().string().c_str());
        ImGui::TextWrapped("Root: %s", project.root.generic_string().c_str());
        ImGui::TextWrapped("Assets: %s", project.assets.generic_string().c_str());
        ImGui::TextWrapped("Cache: %s", project.cache.generic_string().c_str());
        ImGui::TextWrapped("Build: %s", project.build.generic_string().c_str());
        ImGui::Separator();

        ImGui::SetNextItemWidth(-120.0f);
        ImGui::InputText("Startup Scene", m_startup_scene.data(), m_startup_scene.size(), ImGuiInputTextFlags_ReadOnly);
        ImGui::SameLine();
        if (ImGui::Button("Clear"))
            m_startup_scene[0] = '\0';

        if (ctx.project.list_scene_assets && ImGui::BeginCombo("Scene Asset", m_startup_scene[0] ? m_startup_scene.data() : "None"))
        {
            for (const AssetMetadata& metadata : ctx.project.list_scene_assets())
            {
                const bool selected = metadata.source_path == m_startup_scene.data();
                if (ImGui::Selectable(metadata.source_path.c_str(), selected))
                    std::snprintf(m_startup_scene.data(), m_startup_scene.size(), "%s", metadata.source_path.c_str());
                if (selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        if (m_startup_scene[0] == '\0')
            ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.25f, 1.0f), "Player requires --scene while startup scene is empty.");

        if (ImGui::Button("Apply", ImVec2(100.0f, 0.0f)))
        {
            std::string error;
            if (ctx.project.set_startup_scene && ctx.project.set_startup_scene(m_startup_scene.data(), error))
            {
                synchronize();
                m_message = "Project settings saved.";
            }
            else
            {
                m_message = error.empty() ? "Failed to save project settings." : error;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Revert", ImVec2(100.0f, 0.0f)))
            synchronize();
        if (!m_message.empty())
            ImGui::TextWrapped("%s", m_message.c_str());
        ImGui::End();
    }
}
