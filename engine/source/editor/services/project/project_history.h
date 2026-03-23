#pragma once

#include <filesystem>
#include <vector>

namespace Hybrid
{
    class IEditorPlatformServices;

    struct RecentProjectState
    {
        std::vector<std::filesystem::path> recent_project_files;
    };

    class ProjectHistory
    {
    public:
        static bool loadRecentState(const IEditorPlatformServices& platform, RecentProjectState& out_state);
        static bool saveRecentState(const IEditorPlatformServices& platform, const RecentProjectState& state);
        static bool addRecentProject(const IEditorPlatformServices& platform,
                                     const std::filesystem::path& project_file,
                                     size_t max_entries = 8);

    private:
        static std::filesystem::path getStateFilePath(const IEditorPlatformServices& platform);
    };
} // namespace Hybrid
