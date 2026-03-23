#include "project_history.h"

#include <fstream>
#include <algorithm>

#include "editor/services/platform/editor_platform_services.h"
#include <nlohmann/json.hpp>

namespace Hybrid
{
    namespace
    {
        using json = nlohmann::json;
    }

    std::filesystem::path ProjectHistory::getStateFilePath(const IEditorPlatformServices& platform)
    {
        return platform.getEditorUserDataDir() / "recent_projects.json";
    }

    bool ProjectHistory::loadRecentState(const IEditorPlatformServices& platform, RecentProjectState& out_state)
    {
        out_state = {};

        const std::filesystem::path state_file = getStateFilePath(platform);
        if (!std::filesystem::exists(state_file))
            return false;

        std::ifstream ifs(state_file);
        if (!ifs)
            return false;

        json root;
        try
        {
            ifs >> root;
        }
        catch (...)
        {
            return false;
        }

        const json* recent = root.contains("recent_projects") ? &root["recent_projects"] : nullptr;
        if (recent == nullptr || !recent->is_array())
            return false;

        for (const json& item : *recent)
        {
            if (!item.is_string())
                continue;

            const std::string value = item.get<std::string>();
            if (!value.empty())
                out_state.recent_project_files.emplace_back(value);
        }

        return !out_state.recent_project_files.empty();
    }

    bool ProjectHistory::saveRecentState(const IEditorPlatformServices& platform, const RecentProjectState& state)
    {
        const std::filesystem::path state_file = getStateFilePath(platform);
        std::error_code ec;
        std::filesystem::create_directories(state_file.parent_path(), ec);
        if (ec)
            return false;

        std::ofstream ofs(state_file, std::ios::out | std::ios::binary | std::ios::trunc);
        if (!ofs)
            return false;

        json root;
        root["recent_projects"] = json::array();
        for (const auto& path : state.recent_project_files)
        {
            if (!path.empty())
                root["recent_projects"].push_back(path.generic_string());
        }

        ofs << root.dump(2);
        ofs.close();
        return static_cast<bool>(ofs);
    }

    bool ProjectHistory::addRecentProject(const IEditorPlatformServices& platform,
                                          const std::filesystem::path& project_file,
                                          size_t max_entries)
    {
        if (project_file.empty())
            return false;

        RecentProjectState state{};
        (void)loadRecentState(platform, state);

        const std::filesystem::path normalized = project_file.lexically_normal();
        auto& recent = state.recent_project_files;
        recent.erase(std::remove_if(recent.begin(),
                                    recent.end(),
                                    [&normalized](const std::filesystem::path& value)
                                    {
                                        return value.lexically_normal() == normalized;
                                    }),
                     recent.end());
        recent.insert(recent.begin(), normalized);

        if (recent.size() > max_entries)
            recent.resize(max_entries);

        return saveRecentState(platform, state);
    }
} // namespace Hybrid
