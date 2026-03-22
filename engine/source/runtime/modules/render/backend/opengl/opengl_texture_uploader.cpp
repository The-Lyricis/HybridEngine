#include "opengl_texture_uploader.h"

#include <glad/gl.h>

#include "runtime/modules/asset/texture_image.h"
#include "runtime/modules/render/backend/opengl/opengl_texture.h"

namespace Hybrid
{
    namespace
    {
        uint32_t computeMipCount(uint32_t width, uint32_t height)
        {
            uint32_t levels = 1;
            while (width > 1 || height > 1)
            {
                width = width > 1 ? width / 2 : 1;
                height = height > 1 ? height / 2 : 1;
                ++levels;
            }
            return levels;
        }
    } // namespace

    TexturePtr OpenGLTextureUploader::uploadTexture2D(const TextureImageData& image)
    {
        if (!image.isValid() || image.format != TextureFormat::RGBA8)
            return nullptr;

        GLuint tex = 0;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);

        glTexParameteri(GL_TEXTURE_2D,
                        GL_TEXTURE_MIN_FILTER,
                        image.generate_mips ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

        GLint prev_align = 0;
        glGetIntegerv(GL_UNPACK_ALIGNMENT, &prev_align);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

        glTexImage2D(GL_TEXTURE_2D,
                     0,
                     GL_RGBA8,
                     static_cast<GLsizei>(image.width),
                     static_cast<GLsizei>(image.height),
                     0,
                     GL_RGBA,
                     GL_UNSIGNED_BYTE,
                     image.pixels.data());
        if (image.generate_mips)
            glGenerateMipmap(GL_TEXTURE_2D);

        glPixelStorei(GL_UNPACK_ALIGNMENT, prev_align);
        glBindTexture(GL_TEXTURE_2D, 0);

        TextureDesc desc;
        desc.type = TextureType::Tex2D;
        desc.format = TextureFormat::RGBA8;
        desc.width = image.width;
        desc.height = image.height;
        desc.layers = 1;
        desc.mipLevels = image.generate_mips ? computeMipCount(image.width, image.height) : 1;

        return std::make_shared<GLTexture>(tex, desc);
    }
} // namespace Hybrid
