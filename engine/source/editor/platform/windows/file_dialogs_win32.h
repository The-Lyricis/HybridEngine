#pragma once

#include <filesystem>
#include <optional>

#include "editor/services/platform/editor_platform_services.h"

namespace Hybrid
{
    std::optional<std::filesystem::path> ShowSaveFileDialogWin32(
        NativeWindowHandle window,
        const SaveFileDialogDesc& desc);

    std::vector<std::filesystem::path> ShowOpenFileDialogWin32(
        NativeWindowHandle window,
        const OpenFileDialogDesc& desc);

    std::optional<std::filesystem::path> ShowSelectFolderDialogWin32(
        NativeWindowHandle window,
        const SelectFolderDialogDesc& desc);

    bool RevealInFileBrowserWin32(const std::filesystem::path& path);
}
