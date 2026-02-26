#pragma once
#include "asset_loader.h"
#include "material.h"

namespace Hybrid
{
    class StubMaterialLoader : public IAssetLoader<Material>
    {
    public:
        AssetType assetType() const override { return AssetType::Material; }
        std::shared_ptr<Material> load(const AssetMetadata& meta, IVirtualFileSystem& vfs) override;
    };
}
