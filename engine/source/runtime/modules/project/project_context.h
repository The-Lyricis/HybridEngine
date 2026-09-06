#pragma once
#include <filesystem>
#include <string>

namespace Hybrid {

    struct ProjectContext
    {
        int format_version = 2;
        std::string startup_scene;
        std::filesystem::path project_file;
        std::filesystem::path root;
        std::filesystem::path assets;
        std::filesystem::path cache;
        std::filesystem::path build;
        std::filesystem::path settings;

        bool valid() const
        {
            return !project_file.empty() && !root.empty() && !assets.empty() && !cache.empty();
        }
    };

    class ProjectService
    {
    public:
        static void Set(const ProjectContext& ctx) { s_ctx = ctx; }
        static const ProjectContext& Get() { return s_ctx; }

    private:
        static ProjectContext s_ctx;
    };

} // namespace Hybrid
