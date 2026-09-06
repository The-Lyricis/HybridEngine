#pragma once

#include "i_editor_panel.h"

#include <array>
#include <filesystem>
#include <string>

namespace Hybrid
{
    class ProjectSettingsPanel final : public IEditorPanel
    {
    public:
        ProjectSettingsPanel() : IEditorPanel(EditorPanelId::ProjectSettings, "Project Settings", false) {}
        void onImGuiRender(EditorContext& ctx) override;

    private:
        void synchronize();
        std::filesystem::path m_project_file;
        std::array<char, 1024> m_startup_scene{};
        std::string m_message;
    };
}
