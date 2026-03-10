#pragma once

#include <filesystem>
#include <optional>
#include <string>

struct GLFWwindow;

namespace Hybrid
{
    std::optional<std::filesystem::path> ShowSaveSceneDialogWin32(
        GLFWwindow* window,
        const std::filesystem::path& initial_directory,
        const std::wstring& default_file_name);

    bool ShowInExplorerWin32(const std::filesystem::path& path);
}
