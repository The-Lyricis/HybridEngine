#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

struct GLFWwindow;

namespace Hybrid
{
    struct FileDialogFilter
    {
        std::string name;
        std::string pattern;
    };

    struct SaveFileDialogDesc
    {
        std::string title;
        std::filesystem::path initial_dir;
        std::string default_name;
        std::string default_extension;
        std::vector<FileDialogFilter> filters;
    };

    struct OpenFileDialogDesc
    {
        std::string title;
        std::filesystem::path initial_dir;
        std::vector<FileDialogFilter> filters;
        bool allow_multi_select = false;
    };

    struct SelectFolderDialogDesc
    {
        std::string title;
        std::filesystem::path initial_dir;
    };

    class IEditorPlatformServices
    {
    public:
        virtual ~IEditorPlatformServices() = default;

        virtual std::optional<std::filesystem::path>
        showSaveFileDialog(GLFWwindow* parent, const SaveFileDialogDesc& desc) = 0;

        virtual std::vector<std::filesystem::path>
        showOpenFileDialog(GLFWwindow* parent, const OpenFileDialogDesc& desc) = 0;

        virtual std::optional<std::filesystem::path>
        showSelectFolderDialog(GLFWwindow* parent, const SelectFolderDialogDesc& desc) = 0;

        virtual bool revealInFileBrowser(const std::filesystem::path& path) = 0;
    };
} // namespace Hybrid
