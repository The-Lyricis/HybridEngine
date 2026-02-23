#pragma once
#include <filesystem>

namespace Hybrid {

    struct ProjectContext
    {
        std::filesystem::path root;
        std::filesystem::path assets;
        std::filesystem::path cache;
        std::filesystem::path build;
        std::filesystem::path settings;

        bool valid() const
        {
            return !root.empty() && !assets.empty() && !cache.empty();
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
