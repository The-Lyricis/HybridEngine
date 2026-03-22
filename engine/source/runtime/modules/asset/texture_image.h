#pragma once

#include <cstdint>
#include <vector>

#include "runtime/modules/render/public/texture.h"

namespace Hybrid
{
    // CPU-side decoded pixel data for a texture asset.
    // This is intentionally separate from render::Texture, which is the GPU resource.
    struct TextureImageData
    {
        uint32_t width = 0;
        uint32_t height = 0;
        TextureFormat format = TextureFormat::RGBA8;
        bool generate_mips = true;
        std::vector<uint8_t> pixels;

        bool isValid() const
        {
            return width > 0 && height > 0 && !pixels.empty();
        }
    };
} // namespace Hybrid
