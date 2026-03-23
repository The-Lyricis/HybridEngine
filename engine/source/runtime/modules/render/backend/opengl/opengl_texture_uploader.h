#pragma once

#include "runtime/modules/render/public/texture_uploader.h"

namespace Hybrid
{
    class OpenGLTextureUploader final : public TextureUploader
    {
    public:
        TexturePtr uploadTexture2D(const TextureImageData& image) override;
        TexturePtr uploadTextureCube(const CubemapImageData& image) override;
    };
} // namespace Hybrid
