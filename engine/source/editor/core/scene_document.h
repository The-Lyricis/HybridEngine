#pragma once

#include <filesystem>
#include <memory>
#include <string>

namespace Hybrid
{
    class Scene;

    struct SceneDocument
    {
        std::shared_ptr<Scene> scene;
        bool dirty = false;
        std::string vpath;
        std::filesystem::path native_path;
        std::string display_name = "Untitled";

        bool isSaved() const
        {
            return !vpath.empty();
        }
    };
} // namespace Hybrid
