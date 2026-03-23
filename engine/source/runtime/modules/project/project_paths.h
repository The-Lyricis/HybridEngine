#pragma once

#include <filesystem>
#include <string>

namespace Hybrid
{
    bool ResolveProjectFilePath(const std::filesystem::path& requested_path,
                                std::filesystem::path& out_project_file,
                                std::string& out_error);
} // namespace Hybrid
