#pragma once

#include <string>
#include <vector>

#include "asset_loader.h"
#include "texture_image.h"

namespace Hybrid
{
    class TextureImageLoader final : public IAssetLoader<TextureImageData>
    {
    public:
        AssetType assetType() const override { return AssetType::Texture2D; }
        std::shared_ptr<TextureImageData> load(const AssetMetadata& meta, IVirtualFileSystem& vfs) override;

    private:
        std::vector<char> readBytes(const IVirtualFileSystem& vfs, const std::string& path) const;
    };
} // namespace Hybrid
