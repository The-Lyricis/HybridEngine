#pragma once
#include <memory>

#include "asset_loader.h"
#include "material.h"

namespace Hybrid
{
    class MaterialFileLoader : public IAssetLoader<Material>
    {
    public:
        explicit MaterialFileLoader(std::shared_ptr<AssetRegistry> registry)
            : m_registry(std::move(registry))
        {
        }

        AssetType assetType() const override { return AssetType::Material; }
        std::shared_ptr<Material> load(const AssetMetadata& meta, IVirtualFileSystem& vfs) override;

    private:
        std::shared_ptr<AssetRegistry> m_registry;
    };
}
