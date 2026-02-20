#pragma once

#include "i_asset_importer.h"

namespace Hybrid
{
    class TextureImporter final : public IAssetImporter
    {
    public:
        AssetType primaryType() const override { return AssetType::Texture2D; }
        bool supportsExtension(std::string_view ext) const override;
        ImportResult importAsset(const ImportRequest& request, AssetRegistry& registry) override;
    };
} // namespace Hybrid

