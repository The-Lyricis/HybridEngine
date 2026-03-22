#include "editor_platform_services_win32.h"

#include "file_dialogs_win32.h"

namespace Hybrid
{
    std::optional<std::filesystem::path>
    EditorPlatformServicesWin32::showSaveFileDialog(GLFWwindow* parent, const SaveFileDialogDesc& desc)
    {
        return ShowSaveFileDialogWin32(parent, desc);
    }

    std::vector<std::filesystem::path>
    EditorPlatformServicesWin32::showOpenFileDialog(GLFWwindow* parent, const OpenFileDialogDesc& desc)
    {
        return ShowOpenFileDialogWin32(parent, desc);
    }

    std::optional<std::filesystem::path>
    EditorPlatformServicesWin32::showSelectFolderDialog(GLFWwindow* parent, const SelectFolderDialogDesc& desc)
    {
        return ShowSelectFolderDialogWin32(parent, desc);
    }

    bool EditorPlatformServicesWin32::revealInFileBrowser(const std::filesystem::path& path)
    {
        return RevealInFileBrowserWin32(path);
    }
} // namespace Hybrid
