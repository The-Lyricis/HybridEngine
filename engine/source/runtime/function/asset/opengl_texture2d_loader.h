#pragma once

#include <string>
#include <vector>

#include "asset_loader.h"
#include "runtime/core/base/virtual_file_system.h"
#include "runtime/function/render/texture.h" 

namespace Hybrid
{
    using TextureHandle = Hybrid::TexturePtr;

    class GLTexture2DLoader final : public IAssetLoader<Hybrid::Texture>
    {
    public:
        AssetType assetType() const override { return AssetType::Texture2D; }

        TextureHandle load(const AssetMetadata& meta, IVirtualFileSystem& vfs) override;

    private:
        std::vector<char> readBytes(const IVirtualFileSystem& vfs, const std::string& path) const;
    };
} // namespace Hybrid
