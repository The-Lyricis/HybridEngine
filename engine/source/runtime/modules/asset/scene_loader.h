#pragma once
#include <memory>

#include "asset_loader.h"
#include <runtime/modules/scene/scene.h>


namespace Hybrid
{
    class SceneLoader final : public IAssetLoader<Scene>
    {
    public:
        AssetType assetType() const override { return AssetType::Scene; }

        static std::shared_ptr<Scene> loadFromLogicalPath(const std::string& logical, IVirtualFileSystem& vfs);
        std::shared_ptr<Scene> load(const AssetMetadata& meta, IVirtualFileSystem& vfs) override;
    };
} // namespace Hybrid
