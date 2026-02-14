#pragma once

#include <memory>
#include <glad/gl.h>
#include <stb_image.h>

#include "asset_loader.h"
#include "runtime/core/base/virtual_file_system.h"
#include "runtime/core/log/log_system.h"

namespace Hybrid
{
    // 简单纹理包装，便于自定义 deleter
    struct GLTexture
    {
        uint32_t id = 0;
    };
    using GLTextureHandle = std::shared_ptr<GLTexture>;

    class Texture2DLoader final : public IAssetLoader<GLTextureHandle>
    {
    public:
        AssetType assetType() const override { return AssetType::Texture2D; }

        GLTextureHandle load(const AssetMetadata& meta, IVirtualFileSystem& vfs) override;

    private:
        std::vector<char> readBytes(IVirtualFileSystem& vfs, const std::string& path) const;
    };
} // namespace Hybrid
