#pragma once

#include <filesystem>
#include <string>

namespace Hybrid
{
    struct ProjectCreateDesc
    {
        std::filesystem::path project_root;
        std::string project_name;
        std::filesystem::path assets_dir = "Assets";
        std::filesystem::path cache_dir = "Cache";
        std::filesystem::path build_dir = "Build";
        std::filesystem::path settings_dir = "ProjectSettings";
        bool create_hyproj_if_missing = true;
    };

    class ProjectCreator
    {
    public:
        static bool CreateProject(const ProjectCreateDesc& desc,
                                  std::filesystem::path& out_hyproj_path,
                                  std::string& out_error);
    };
} // namespace Hybrid
