#pragma once

#include <string>
#include <vector>

#include "asset_loader.h"
#include "cubemap_image.h"

namespace Hybrid
{
    class CubemapImageLoader final : public IAssetLoader<CubemapImageData>
    {
    public:
        AssetType assetType() const override { return AssetType::TextureCube; }
        std::shared_ptr<CubemapImageData> load(const AssetMetadata& meta, IVirtualFileSystem& vfs) override;

    private:
        std::vector<char> readBytes(const IVirtualFileSystem& vfs, const std::string& path) const;
    };
} // namespace Hybrid
