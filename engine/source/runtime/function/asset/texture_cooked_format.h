#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace Hybrid
{
    constexpr uint32_t HTEX_FLAG_NONE = 0;

    enum class HtexPixelFormat : uint16_t
    {
        RGBA8 = 1
    };

    struct HtexImage
    {
        HtexPixelFormat format = HtexPixelFormat::RGBA8;
        uint32_t width = 0;
        uint32_t height = 0;
        uint8_t channels = 4;
        uint8_t mip_count = 1;
        uint32_t flags = HTEX_FLAG_NONE;
        uint32_t row_stride = 0;
        std::vector<uint8_t> pixels;
    };

    bool HtexEncodeRgba8(uint32_t width,
                         uint32_t height,
                         const uint8_t* rgba_pixels,
                         uint32_t flags,
                         std::vector<char>& out_bytes);

    bool HtexDecode(const std::vector<char>& bytes, HtexImage& out_image, std::string* out_error = nullptr);
    bool HtexLooksLikeFile(const std::vector<char>& bytes);
} // namespace Hybrid
