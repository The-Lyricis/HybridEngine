#pragma once

namespace Hybrid
{
    // Backend-independent pixel format shared by decoded CPU images and GPU textures.
    enum class TextureFormat
    {
        Unknown = 0,
        RGB8,
        RGBA8,
    };
} // namespace Hybrid
