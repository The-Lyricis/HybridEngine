#pragma once

#include <filesystem>
#include <memory>
#include <string>

#include "runtime/modules/asset/asset_type.h"

namespace Hybrid
{
    class Scene;

    struct SceneDocument
    {
        std::shared_ptr<Scene> scene;
        bool dirty = false;
        AssetID scene_asset_id{};
        std::string vpath;
        std::filesystem::path native_path;
        std::string display_name = "Untitled";

        bool isSaved() const
        {
            return !vpath.empty();
        }
    };
} // namespace Hybrid
