#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "runtime/modules/render/public/texture.h"

namespace Hybrid
{
    struct CubemapFaceImage
    {
        std::vector<uint8_t> pixels;

        bool isValid() const
        {
            return !pixels.empty();
        }
    };

    // CPU-side decoded pixel data for a cubemap texture asset.
    // Face order follows OpenGL cubemap face order: +X, -X, +Y, -Y, +Z, -Z.
    struct CubemapImageData
    {
        uint32_t width = 0;
        uint32_t height = 0;
        TextureFormat format = TextureFormat::RGBA8;
        bool srgb = false;
        bool generate_mips = false;
        std::array<CubemapFaceImage, 6> faces{};

        bool isValid() const
        {
            if (width == 0 || height == 0)
                return false;

            for (const CubemapFaceImage& face : faces)
            {
                if (!face.isValid())
                    return false;
            }

            return true;
        }
    };
} // namespace Hybrid
