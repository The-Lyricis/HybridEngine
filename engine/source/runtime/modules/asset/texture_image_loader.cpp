#include "texture_image_loader.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <cstring>

#include "runtime/core/base/macro.h"
#include "texture_cooked_format.h"

#if !defined(HYBRID_ALLOW_SOURCE_FALLBACK)
#if defined(NDEBUG)
#define HYBRID_ALLOW_SOURCE_FALLBACK 0
#else
#define HYBRID_ALLOW_SOURCE_FALLBACK 1
#endif
#endif

namespace Hybrid
{
    namespace
    {
        constexpr const char* kTextureImageLoaderLogTag = "[TextureImageLoader]";

        std::shared_ptr<TextureImageData> makeTextureImageFromRgba8(const uint8_t* pixels, int w, int h)
        {
            if (!pixels || w <= 0 || h <= 0)
                return nullptr;

            auto image = std::make_shared<TextureImageData>();
            image->width = static_cast<uint32_t>(w);
            image->height = static_cast<uint32_t>(h);
            image->format = TextureFormat::RGBA8;
            image->generate_mips = true;
            image->pixels.resize(static_cast<size_t>(w) * static_cast<size_t>(h) * 4u);

            const size_t row_bytes = static_cast<size_t>(w) * 4u;
            for (int y = 0; y < h; ++y)
            {
                const uint8_t* src_row = pixels + static_cast<size_t>(y) * row_bytes;
                uint8_t* dst_row = image->pixels.data() + static_cast<size_t>(h - 1 - y) * row_bytes;
                std::memcpy(dst_row, src_row, row_bytes);
            }

            return image;
        }
    } // namespace

    std::vector<char> TextureImageLoader::readBytes(const IVirtualFileSystem& vfs, const std::string& path) const
    {
        if (path.empty())
            return {};
        return vfs.readAll(path);
    }

    std::shared_ptr<TextureImageData> TextureImageLoader::load(const AssetMetadata& meta, IVirtualFileSystem& vfs)
    {
        const std::string cooked = meta.cooked_path;
        const std::string source = meta.source_path;
        constexpr bool kAllowSourceFallback = HYBRID_ALLOW_SOURCE_FALLBACK != 0;

        if (cooked.empty() && !kAllowSourceFallback)
        {
            HBD_CORE_ERROR("{} load_failed asset_id={} source_path={} reason=empty_cooked_path_fallback_disabled",
                           kTextureImageLoaderLogTag,
                           meta.id.value,
                           source.empty() ? "<empty>" : source);
            return nullptr;
        }

        if (!cooked.empty())
        {
            std::vector<char> cooked_bytes = readBytes(vfs, cooked);
            if (cooked_bytes.empty())
            {
                HBD_CORE_WARN("{} cooked_missing asset_id={} cooked_path={}",
                              kTextureImageLoaderLogTag,
                              meta.id.value,
                              cooked);
            }
            else
            {
                HtexImage decoded{};
                std::string decode_error;
                if (HtexDecode(cooked_bytes, decoded, &decode_error))
                {
                    auto image = makeTextureImageFromRgba8(decoded.pixels.data(),
                                                          static_cast<int>(decoded.width),
                                                          static_cast<int>(decoded.height));
                    if (!image)
                    {
                        HBD_CORE_ERROR("{} load_failed asset_id={} cooked_path={} reason=image_build_failed",
                                       kTextureImageLoaderLogTag,
                                       meta.id.value,
                                       cooked);
                    }
                    return image;
                }

                HBD_CORE_ERROR("{} load_failed asset_id={} cooked_path={} reason=decode_failed error={}",
                               kTextureImageLoaderLogTag,
                               meta.id.value,
                               cooked,
                               decode_error.empty() ? "<empty>" : decode_error);
                if (!HtexLooksLikeFile(cooked_bytes))
                {
                    HBD_CORE_ERROR("{} load_failed asset_id={} cooked_path={} reason=format_mismatch",
                                   kTextureImageLoaderLogTag,
                                   meta.id.value,
                                   cooked);
                }
            }

            if (!kAllowSourceFallback)
            {
                HBD_CORE_ERROR("{} load_failed asset_id={} source_path={} reason=cooked_unusable_fallback_disabled",
                               kTextureImageLoaderLogTag,
                               meta.id.value,
                               source.empty() ? "<empty>" : source);
                return nullptr;
            }
        }

        std::vector<char> source_bytes = readBytes(vfs, source);
        if (source_bytes.empty())
        {
            HBD_CORE_ERROR("{} load_failed asset_id={} source_path={} reason=missing_source_data",
                           kTextureImageLoaderLogTag,
                           meta.id.value,
                           source.empty() ? "<empty>" : source);
            return nullptr;
        }

        int w = 0;
        int h = 0;
        int comp = 0;
        stbi_uc* pixels = stbi_load_from_memory(reinterpret_cast<const stbi_uc*>(source_bytes.data()),
                                                static_cast<int>(source_bytes.size()),
                                                &w,
                                                &h,
                                                &comp,
                                                4);
        if (!pixels || w <= 0 || h <= 0)
        {
            HBD_CORE_ERROR("{} load_failed asset_id={} source_path={} reason=source_decode_failed",
                           kTextureImageLoaderLogTag,
                           meta.id.value,
                           source.empty() ? "<empty>" : source);
            if (pixels)
                stbi_image_free(pixels);
            return nullptr;
        }

        auto image = makeTextureImageFromRgba8(reinterpret_cast<const uint8_t*>(pixels), w, h);
        stbi_image_free(pixels);

        if (image)
        {
            HBD_CORE_WARN("{} source_fallback_succeeded asset_id={} source_path={}",
                          kTextureImageLoaderLogTag,
                          meta.id.value,
                          source.empty() ? "<empty>" : source);
        }
        return image;
    }
} // namespace Hybrid
