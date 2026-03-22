#pragma once

#include "editor/services/platform/editor_platform_services.h"

namespace Hybrid
{
    class EditorPlatformServicesWin32 final : public IEditorPlatformServices
    {
    public:
        std::optional<std::filesystem::path>
        showSaveFileDialog(GLFWwindow* parent, const SaveFileDialogDesc& desc) override;

        std::vector<std::filesystem::path>
        showOpenFileDialog(GLFWwindow* parent, const OpenFileDialogDesc& desc) override;

        std::optional<std::filesystem::path>
        showSelectFolderDialog(GLFWwindow* parent, const SelectFolderDialogDesc& desc) override;

        bool revealInFileBrowser(const std::filesystem::path& path) override;
    };
} // namespace Hybrid
