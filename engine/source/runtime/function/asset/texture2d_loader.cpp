#include "texture2d_loader.h"
#include "runtime/core/base/macro.h"
#include <stb_image.h>
#include <stdexcept>

namespace Hybrid
{
    std::vector<char> Texture2DLoader::readBytes(IVirtualFileSystem& vfs, const std::string& path) const
    {
        if (path.empty())
            return {};
        return vfs.readAll(path);
    }

    GLTextureHandle Texture2DLoader::load(const AssetMetadata& meta, IVirtualFileSystem& vfs)
    {
        // 防护：检测当前线程是否有 GL 上下文（以绑定纹理对象的可用性为准）
        GLint currentTex = 0;
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &currentTex);
        if (glGetError() != GL_NO_ERROR)
        {
            HBD_CORE_ERROR("Texture2DLoader called without a valid OpenGL context (meta: {})", meta.source_path);
            // 清除错误标志，避免污染后续调用
            while (glGetError() != GL_NO_ERROR) {}
            return nullptr;
        }

        // 优先尝试 cooked，失败回源文件
        std::vector<char> bytes = readBytes(vfs, meta.cooked_path.empty() ? meta.source_path : meta.cooked_path);
        if (bytes.empty() && !meta.cooked_path.empty())
            bytes = readBytes(vfs, meta.source_path);
        if (bytes.empty())
        {
            HBD_CORE_ERROR("Texture2D load failed: {} (no data)", meta.source_path.string());
            return nullptr;
        }

        int w = 0, h = 0, comp = 0;
        // 强制输出 4 通道，避免 UNPACK_ALIGNMENT 问题
        stbi_uc* pixels = stbi_load_from_memory(reinterpret_cast<const stbi_uc*>(bytes.data()),
                                                static_cast<int>(bytes.size()), &w, &h, &comp, 4);
        if (!pixels)
        {
            HBD_CORE_ERROR("stb_image decode failed: {}", meta.source_path.string());
            return nullptr;
        }

        GLenum format = GL_RGBA;
        GLenum internal = GL_RGBA8;

        GLuint tex = 0;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexImage2D(GL_TEXTURE_2D, 0, internal, w, h, 0, format, GL_UNSIGNED_BYTE, pixels);
        glGenerateMipmap(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, 0);
        stbi_image_free(pixels);

        // 自动释放 GL 纹理
        return GLTextureHandle(new GLTexture{tex}, [](GLTexture* t) {
            if (t && t->id)
                glDeleteTextures(1, &t->id);
            delete t;
        });
    }
} // namespace Hybrid
