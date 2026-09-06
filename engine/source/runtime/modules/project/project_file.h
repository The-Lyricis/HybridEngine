#pragma once

#include <filesystem>
#include <string>

namespace Hybrid
{
    class ProjectFile
    {
    public:
        // Atomically updates one key while preserving comments, order and unknown fields.
        static bool updateValue(const std::filesystem::path& project_file,
                                const std::string& key,
                                const std::string& value,
                                std::string& out_error);
    };
}
