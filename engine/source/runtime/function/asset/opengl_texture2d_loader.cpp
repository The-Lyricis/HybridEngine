#include "opengl_texture2d_loader.h"

#include <glad/gl.h>
#include <stb_image.h>

#include "runtime/core/base/macro.h"
#include "runtime/core/log/log_system.h"
#include "runtime/function/render/opengl/opengl_texture.h"

namespace Hybrid
{
    std::vector<char> GLTexture2DLoader::readBytes(const IVirtualFileSystem& vfs, const std::string& path) const
    {
        if (path.empty())
            return {};
        return vfs.readAll(path);
    }

    TextureHandle GLTexture2DLoader::load(const AssetMetadata& meta, IVirtualFileSystem& vfs)
    {
        // GL 上下文防护：必须在持有 OpenGL 上下文的线程调用
        GLint currentTex = 0;
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &currentTex);
        if (glGetError() != GL_NO_ERROR)
        {
            HBD_CORE_ERROR("GLTexture2DLoader: no valid GL context for {}", meta.source_path);
            while (glGetError() != GL_NO_ERROR) {}
            return nullptr;
        }

        // 1) 读取数据：优先 cooked，失败回源文件
        const std::string cooked = meta.cooked_path;
        const std::string source = meta.source_path;

        std::vector<char> bytes = readBytes(vfs, cooked.empty() ? source : cooked);
        if (bytes.empty() && !cooked.empty())
            bytes = readBytes(vfs, source);

        if (bytes.empty())
        {
            HBD_CORE_ERROR("Texture2D load failed: {} (no data)", source);
            return nullptr;
        }

        // 2) stb 解码（强制 4 通道，避免 UNPACK 对齐问题）
        int w = 0, h = 0, comp = 0;
        stbi_uc* pixels = stbi_load_from_memory(reinterpret_cast<const stbi_uc*>(bytes.data()),
                                                static_cast<int>(bytes.size()), &w, &h, &comp, 4);

        if (!pixels || w <= 0 || h <= 0)
        {
            HBD_CORE_ERROR("stb_image decode failed: {}", source);
            if (pixels) stbi_image_free(pixels);
            return nullptr;
        }

        // 3) OpenGL 创建与上传
        GLuint tex = 0;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);

        GLint prevAlign = 0;
        glGetIntegerv(GL_UNPACK_ALIGNMENT, &prevAlign);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

        const GLenum format   = GL_RGBA;
        const GLenum internal = GL_RGBA8; // 如需 sRGB，可改为 GL_SRGB8_ALPHA8

        glTexImage2D(GL_TEXTURE_2D, 0, internal, w, h, 0, format, GL_UNSIGNED_BYTE, pixels);
        glGenerateMipmap(GL_TEXTURE_2D);

        glPixelStorei(GL_UNPACK_ALIGNMENT, prevAlign);
        glBindTexture(GL_TEXTURE_2D, 0);

        stbi_image_free(pixels);

        // 4) 返回纹理对象（GLTexture 实现 Texture 抽象）
        TextureDesc desc;
        desc.type   = TextureType::Tex2D;
        desc.format = TextureFormat::RGBA8;
        desc.width  = static_cast<uint32_t>(w);
        desc.height = static_cast<uint32_t>(h);
        desc.layers = 1;
        desc.mipLevels = 1; // 当前生成完整 mip 链，实际 mip 数取决于 glGenerateMipmap

        return std::make_shared<GLTexture>(tex, desc);
    }
} // namespace Hybrid
