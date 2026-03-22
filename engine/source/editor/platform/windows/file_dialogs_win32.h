#pragma once

#include <filesystem>
#include <optional>

#include "editor/services/platform/editor_platform_services.h"

struct GLFWwindow;

namespace Hybrid
{
    std::optional<std::filesystem::path> ShowSaveFileDialogWin32(
        GLFWwindow* window,
        const SaveFileDialogDesc& desc);

    std::vector<std::filesystem::path> ShowOpenFileDialogWin32(
        GLFWwindow* window,
        const OpenFileDialogDesc& desc);

    std::optional<std::filesystem::path> ShowSelectFolderDialogWin32(
        GLFWwindow* window,
        const SelectFolderDialogDesc& desc);

    bool RevealInFileBrowserWin32(const std::filesystem::path& path);
}
