#include "project_paths.h"

#include <algorithm>
#include <vector>

namespace Hybrid
{
    bool ResolveProjectFilePath(const std::filesystem::path& requested_path,
                                std::filesystem::path& out_project_file,
                                std::string& out_error)
    {
        namespace fs = std::filesystem;

        if (requested_path.empty())
        {
            out_error = "project path is empty";
            return false;
        }

        const fs::path absolute_path = fs::absolute(requested_path);
        if (fs::is_regular_file(absolute_path))
        {
            if (absolute_path.extension() != ".hyproj")
            {
                out_error = "project file must use the .hyproj extension: " + absolute_path.string();
                return false;
            }

            std::error_code ec;
            out_project_file = fs::weakly_canonical(absolute_path, ec);
            if (ec)
            {
                out_error = "failed to normalize project file path: " + absolute_path.string() + " (" + ec.message() + ")";
                return false;
            }
            return true;
        }

        if (fs::is_directory(absolute_path))
        {
            std::vector<fs::path> hyproj_files;
            std::error_code ec;
            for (const auto& entry : fs::directory_iterator(absolute_path, ec))
            {
                if (ec)
                {
                    out_error = "failed to enumerate project directory: " + absolute_path.string() + " (" + ec.message() + ")";
                    return false;
                }

                if (entry.is_regular_file() && entry.path().extension() == ".hyproj")
                    hyproj_files.push_back(entry.path());
            }

            if (hyproj_files.empty())
            {
                out_error = "no .hyproj file found in directory: " + absolute_path.string();
                return false;
            }

            std::sort(hyproj_files.begin(), hyproj_files.end());
            std::error_code canonical_ec;
            out_project_file = fs::weakly_canonical(hyproj_files.front(), canonical_ec);
            if (canonical_ec)
            {
                out_error = "failed to normalize project file path: " + hyproj_files.front().string() + " (" + canonical_ec.message() + ")";
                return false;
            }
            return true;
        }

        out_error = "project path does not exist: " + absolute_path.string();
        return false;
    }
} // namespace Hybrid
