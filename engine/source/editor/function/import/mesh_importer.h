#pragma once

#include "i_asset_importer.h"

namespace Hybrid
{
    class MeshImporter final : public IAssetImporter
    {
    public:
        AssetType primaryType() const override { return AssetType::Mesh; }
        bool supportsExtension(std::string_view ext) const override;
        ImportResult importAsset(const ImportRequest& request, AssetRegistry& registry) override;
    };
} // namespace Hybrid

