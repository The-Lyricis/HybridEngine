#pragma once

#include "editor/services/platform/editor_platform_services.h"

namespace Hybrid
{
    class EditorPlatformServicesWin32 final : public IEditorPlatformServices
    {
    public:
        std::filesystem::path getEditorUserDataDir() const override;
        std::filesystem::path getCurrentExecutablePath() const override;
        bool launchEditorProcess(const std::filesystem::path& editor_executable,
                                 const std::vector<std::string>& args) const override;

        std::optional<std::filesystem::path>
        showSaveFileDialog(GLFWwindow* parent, const SaveFileDialogDesc& desc) override;

        std::vector<std::filesystem::path>
        showOpenFileDialog(GLFWwindow* parent, const OpenFileDialogDesc& desc) override;

        std::optional<std::filesystem::path>
        showSelectFolderDialog(GLFWwindow* parent, const SelectFolderDialogDesc& desc) override;

        bool revealInFileBrowser(const std::filesystem::path& path) override;
    };
} // namespace Hybrid
