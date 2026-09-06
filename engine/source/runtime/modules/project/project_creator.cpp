#include "project_creator.h"

#include <fstream>

namespace Hybrid
{
    ProjectCreateDesc MakeDebugBootstrapProjectDesc(const std::filesystem::path& output_dir)
    {
        ProjectCreateDesc desc{};
        desc.project_root = output_dir / "GameProject";
        desc.project_name = "GameProject";
        return desc;
    }

    namespace
    {
        std::string pathToString(const std::filesystem::path& path)
        {
            return path.generic_string();
        }
    } // namespace

    bool ProjectCreator::CreateProject(const ProjectCreateDesc& desc,
                                       std::filesystem::path& out_hyproj_path,
                                       std::string& out_error)
    {
        namespace fs = std::filesystem;

        if (desc.project_root.empty())
        {
            out_error = "project root is empty";
            return false;
        }

        if (desc.project_name.empty())
        {
            out_error = "project name is empty";
            return false;
        }

        const fs::path project_root = fs::absolute(desc.project_root);
        const fs::path hyproj_path = project_root / (desc.project_name + ".hyproj");
        const fs::path assets_dir = project_root / desc.assets_dir;
        const fs::path cache_dir = project_root / desc.cache_dir;
        const fs::path build_dir = project_root / desc.build_dir;
        const fs::path settings_dir = project_root / desc.settings_dir;

        const auto create_directory = [&out_error](const char* label, const fs::path& path)
        {
            std::error_code ec;
            fs::create_directories(path, ec);
            if (ec)
            {
                out_error = std::string("failed to create ") + label + ": " + pathToString(path) + " (" + ec.message() + ")";
                return false;
            }
            return true;
        };

        if (!create_directory("project root", project_root) ||
            !create_directory("assets directory", assets_dir) ||
            !create_directory("cache directory", cache_dir) ||
            !create_directory("build directory", build_dir) ||
            !create_directory("settings directory", settings_dir))
        {
            return false;
        }

        if (desc.create_hyproj_if_missing && !fs::exists(hyproj_path))
        {
            std::ofstream ofs(hyproj_path, std::ios::out | std::ios::binary);
            if (!ofs)
            {
                out_error = "failed to open project file for write: " + pathToString(hyproj_path);
                return false;
            }

            ofs << "# Auto-generated project file\n";
            ofs << "format_version=2\n";
            ofs << "name=" << desc.project_name << "\n";
            ofs << "startup_scene=\n";
            ofs << "assets=" << desc.assets_dir.generic_string() << "\n";
            ofs << "cache=" << desc.cache_dir.generic_string() << "\n";
            ofs << "build=" << desc.build_dir.generic_string() << "\n";
            ofs << "settings=" << desc.settings_dir.generic_string() << "\n";
            ofs.close();

            if (!ofs)
            {
                out_error = "failed to finalize project file: " + pathToString(hyproj_path);
                return false;
            }
        }

        out_hyproj_path = hyproj_path;
        return true;
    }
} // namespace Hybrid
