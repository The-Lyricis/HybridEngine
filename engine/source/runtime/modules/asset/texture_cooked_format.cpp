#include "texture_cooked_format.h"

#include <cstring>

namespace Hybrid
{
    namespace
    {
        constexpr char kMagic[4] = {'H', 'T', 'E', 'X'};
        constexpr uint16_t kVersion = 1;

        struct HtexHeaderV1
        {
            char magic[4];
            uint16_t version;
            uint16_t format;
            uint32_t width;
            uint32_t height;
            uint8_t channels;
            uint8_t mip_count;
            uint16_t reserved;
            uint32_t flags;
            uint32_t row_stride;
            uint32_t data_size;
        };

        static_assert(sizeof(HtexHeaderV1) == 32, "Unexpected HTEX header size");

        void setError(std::string* out_error, const char* msg)
        {
            if (out_error)
                *out_error = msg;
        }
    } // namespace

    bool HtexEncodeRgba8(uint32_t width,
                         uint32_t height,
                         const uint8_t* rgba_pixels,
                         uint32_t flags,
                         std::vector<char>& out_bytes)
    {
        if (!rgba_pixels || width == 0 || height == 0)
            return false;

        const uint64_t bytes64 = static_cast<uint64_t>(width) * static_cast<uint64_t>(height) * 4ull;
        if (bytes64 > 0xFFFFFFFFull)
            return false;

        HtexHeaderV1 header{};
        std::memcpy(header.magic, kMagic, sizeof(kMagic));
        header.version = kVersion;
        header.format = static_cast<uint16_t>(HtexPixelFormat::RGBA8);
        header.width = width;
        header.height = height;
        header.channels = 4;
        header.mip_count = 1;
        header.reserved = 0;
        header.flags = flags;
        header.row_stride = width * 4;
        header.data_size = static_cast<uint32_t>(bytes64);

        out_bytes.resize(sizeof(HtexHeaderV1) + header.data_size);
        std::memcpy(out_bytes.data(), &header, sizeof(HtexHeaderV1));
        std::memcpy(out_bytes.data() + sizeof(HtexHeaderV1), rgba_pixels, header.data_size);
        return true;
    }

    bool HtexDecode(const std::vector<char>& bytes, HtexImage& out_image, std::string* out_error)
    {
        if (bytes.size() < sizeof(HtexHeaderV1))
        {
            setError(out_error, "HTEX decode failed: file too small");
            return false;
        }

        HtexHeaderV1 header{};
        std::memcpy(&header, bytes.data(), sizeof(HtexHeaderV1));

        if (std::memcmp(header.magic, kMagic, sizeof(kMagic)) != 0)
        {
            setError(out_error, "HTEX decode failed: bad magic");
            return false;
        }
        if (header.version != kVersion)
        {
            setError(out_error, "HTEX decode failed: unsupported version");
            return false;
        }
        if (header.format != static_cast<uint16_t>(HtexPixelFormat::RGBA8))
        {
            setError(out_error, "HTEX decode failed: unsupported format");
            return false;
        }
        if (header.channels != 4 || header.mip_count != 1)
        {
            setError(out_error, "HTEX decode failed: unsupported channels/mips");
            return false;
        }
        if (header.width == 0 || header.height == 0)
        {
            setError(out_error, "HTEX decode failed: invalid size");
            return false;
        }
        if (header.row_stride != header.width * 4)
        {
            setError(out_error, "HTEX decode failed: invalid row stride");
            return false;
        }

        const size_t expected = sizeof(HtexHeaderV1) + static_cast<size_t>(header.data_size);
        if (bytes.size() != expected)
        {
            setError(out_error, "HTEX decode failed: data size mismatch");
            return false;
        }
        if (header.data_size != header.width * header.height * 4)
        {
            setError(out_error, "HTEX decode failed: invalid payload size");
            return false;
        }

        out_image.format = HtexPixelFormat::RGBA8;
        out_image.width = header.width;
        out_image.height = header.height;
        out_image.channels = header.channels;
        out_image.mip_count = header.mip_count;
        out_image.flags = header.flags;
        out_image.row_stride = header.row_stride;
        out_image.pixels.resize(header.data_size);
        std::memcpy(out_image.pixels.data(), bytes.data() + sizeof(HtexHeaderV1), header.data_size);
        return true;
    }

    bool HtexLooksLikeFile(const std::vector<char>& bytes)
    {
        return bytes.size() >= 4 && std::memcmp(bytes.data(), kMagic, sizeof(kMagic)) == 0;
    }
} // namespace Hybrid
