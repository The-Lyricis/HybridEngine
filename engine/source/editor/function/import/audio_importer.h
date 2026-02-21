#pragma once

#include "i_asset_importer.h"

namespace Hybrid
{
    class AudioImporter final : public IAssetImporter
    {
    public:
        AssetType primaryType() const override { return AssetType::Audio; }
        bool supportsExtension(std::string_view ext) const override;
        ImportResult importAsset(const ImportRequest& request,
                                 AssetRegistry& registry,
                                 IVirtualFileSystem& vfs) override;
    };
} // namespace Hybrid

