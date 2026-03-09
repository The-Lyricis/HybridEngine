#include <glad/gl.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <cstring>
#include <vector>

#include "runtime/core/base/macro.h"
#include "runtime/core/log/log_system.h"
#include "runtime/modules/render/backend/opengl/opengl_texture.h"
#include "opengl_texture2d_loader.h"
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
        TextureHandle CreateTextureFromRgba8(const uint8_t* pixels, int w, int h)
        {
            if (!pixels || w <= 0 || h <= 0)
                return nullptr;

            std::vector<uint8_t> flipped(static_cast<size_t>(w) * static_cast<size_t>(h) * 4u);
            const size_t row_bytes = static_cast<size_t>(w) * 4u;
            for (int y = 0; y < h; ++y)
            {
                const uint8_t* src_row = pixels + static_cast<size_t>(y) * row_bytes;
                uint8_t* dst_row = flipped.data() + static_cast<size_t>(h - 1 - y) * row_bytes;
                std::memcpy(dst_row, src_row, row_bytes);
            }

            GLuint tex = 0;
            glGenTextures(1, &tex);
            glBindTexture(GL_TEXTURE_2D, tex);

            GLint prev_align = 0;
            glGetIntegerv(GL_UNPACK_ALIGNMENT, &prev_align);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, flipped.data());
            glGenerateMipmap(GL_TEXTURE_2D);

            glPixelStorei(GL_UNPACK_ALIGNMENT, prev_align);
            glBindTexture(GL_TEXTURE_2D, 0);

            TextureDesc desc;
            desc.type = TextureType::Tex2D;
            desc.format = TextureFormat::RGBA8;
            desc.width = static_cast<uint32_t>(w);
            desc.height = static_cast<uint32_t>(h);
            desc.layers = 1;
            desc.mipLevels = 1;
            return std::make_shared<GLTexture>(tex, desc);
        }
    } // namespace

    std::vector<char> GLTexture2DLoader::readBytes(const IVirtualFileSystem& vfs, const std::string& path) const
    {
        if (path.empty())
            return {};
        return vfs.readAll(path);
    }

    TextureHandle GLTexture2DLoader::load(const AssetMetadata& meta, IVirtualFileSystem& vfs)
    {
        GLint current_tex = 0;
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &current_tex);
        if (glGetError() != GL_NO_ERROR)
        {
            HBD_CORE_ERROR("GLTexture2DLoader: no valid GL context for {}", meta.source_path);
            while (glGetError() != GL_NO_ERROR) {}
            return nullptr;
        }

        const std::string cooked = meta.cooked_path;
        const std::string source = meta.source_path;
        constexpr bool kAllowSourceFallback = HYBRID_ALLOW_SOURCE_FALLBACK != 0;

        if (cooked.empty() && !kAllowSourceFallback)
        {
            HBD_CORE_ERROR("Texture2D load failed: cooked path empty and fallback disabled for {}", source);
            return nullptr;
        }

        if (!cooked.empty())
        {
            std::vector<char> cooked_bytes = readBytes(vfs, cooked);
            if (cooked_bytes.empty())
            {
                HBD_CORE_WARN("Texture2D cooked missing: {}", cooked);
            }
            else
            {
                HtexImage image{};
                std::string decode_error;
                if (HtexDecode(cooked_bytes, image, &decode_error))
                {
                    TextureHandle tex = CreateTextureFromRgba8(image.pixels.data(),
                                                               static_cast<int>(image.width),
                                                               static_cast<int>(image.height));
                    if (!tex)
                    {
                        HBD_CORE_ERROR("Texture2D cooked upload failed: {}", cooked);
                    }
                    return tex;
                }

                HBD_CORE_ERROR("Texture2D cooked invalid: {} ({})", cooked, decode_error);
                if (!HtexLooksLikeFile(cooked_bytes))
                {
                    HBD_CORE_ERROR("Texture2D cooked format mismatch: {}", cooked);
                }
            }

            if (!kAllowSourceFallback)
            {
                HBD_CORE_ERROR("Texture2D load failed: fallback disabled, cooked unusable for {}", source);
                return nullptr;
            }
        }

        std::vector<char> source_bytes = readBytes(vfs, source);
        if (source_bytes.empty())
        {
            HBD_CORE_ERROR("Texture2D load failed: {} (no source data)", source);
            return nullptr;
        }

        int w = 0, h = 0, comp = 0;
        stbi_uc* pixels = stbi_load_from_memory(reinterpret_cast<const stbi_uc*>(source_bytes.data()),
                                                static_cast<int>(source_bytes.size()),
                                                &w,
                                                &h,
                                                &comp,
                                                4);
        if (!pixels || w <= 0 || h <= 0)
        {
            HBD_CORE_ERROR("Texture2D source decode failed: {}", source);
            if (pixels)
                stbi_image_free(pixels);
            return nullptr;
        }

        TextureHandle out = CreateTextureFromRgba8(reinterpret_cast<const uint8_t*>(pixels), w, h);
        stbi_image_free(pixels);

        if (out)
        {
            HBD_CORE_WARN("Texture2D fallback to source succeeded: {}", source);
        }
        return out;
    }
} // namespace Hybrid

