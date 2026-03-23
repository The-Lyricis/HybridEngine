#pragma once

#include <vector>

#include <glad/gl.h>

#include "runtime/modules/render/public/texture.h"

namespace Hybrid
{
    class GLTexture final : public Texture
    {
    public:
        static TexturePtr Create(const TextureDesc& desc,
                                 const void* data = nullptr,
                                 std::size_t bytes = 0);

        GLTexture(uint32_t id, const TextureDesc& desc)
            : m_id(id), m_desc(desc)
        {
            switch (desc.type)
            {
            case TextureType::Tex2D:
                m_target = GL_TEXTURE_2D;
                break;
            case TextureType::Cube:
                m_target = GL_TEXTURE_CUBE_MAP;
                break;
            case TextureType::Tex2DArray:
                m_target = GL_TEXTURE_2D_ARRAY;
                break;
            default:
                m_target = GL_TEXTURE_2D;
                break;
            }
        }

        ~GLTexture() override
        {
            if (m_id)
                glDeleteTextures(1, &m_id);
        }

        const TextureDesc& getDesc() const override { return m_desc; }
        uint32_t getWidth() const override { return m_desc.width; }
        uint32_t getHeight() const override { return m_desc.height; }
        TextureFormat getFormat() const override { return m_desc.format; }

        void bind(uint32_t slot = 0) const override
        {
            glActiveTexture(GL_TEXTURE0 + slot);
            glBindTexture(m_target, m_id);
        }

        uint32_t id() const { return m_id; }

    private:
        uint32_t m_id = 0;
        TextureDesc m_desc{};
        GLenum m_target = GL_TEXTURE_2D;
    };

    inline GLenum getTextureExternalFormat(TextureFormat format)
    {
        switch (format)
        {
        case TextureFormat::RGBA8:
            return GL_RGBA;
        case TextureFormat::RGB8:
            return GL_RGB;
        default:
            return GL_RGBA;
        }
    }

    inline GLenum getTextureInternalFormat(TextureFormat format, bool srgb)
    {
        switch (format)
        {
        case TextureFormat::RGBA8:
            return srgb ? GL_SRGB8_ALPHA8 : GL_RGBA8;
        case TextureFormat::RGB8:
            return srgb ? GL_SRGB8 : GL_RGB8;
        default:
            return srgb ? GL_SRGB8_ALPHA8 : GL_RGBA8;
        }
    }

    inline TexturePtr GLTexture::Create(const TextureDesc& desc, const void* data, std::size_t /*bytes*/)
    {
        GLuint tex = 0;
        GLenum target = GL_TEXTURE_2D;
        switch (desc.type)
        {
        case TextureType::Tex2D:
            target = GL_TEXTURE_2D;
            break;
        case TextureType::Cube:
            target = GL_TEXTURE_CUBE_MAP;
            break;
        case TextureType::Tex2DArray:
            target = GL_TEXTURE_2D_ARRAY;
            break;
        default:
            return nullptr;
        }

        glGenTextures(1, &tex);
        glBindTexture(target, tex);

        glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        if (target == GL_TEXTURE_CUBE_MAP)
        {
            glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(target, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        }
        else
        {
            glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_REPEAT);
        }

        const GLenum format = getTextureExternalFormat(desc.format);
        const GLenum internal = getTextureInternalFormat(desc.format, desc.srgb);

        if (target == GL_TEXTURE_2D)
        {
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            glTexImage2D(GL_TEXTURE_2D,
                         0,
                         internal,
                         static_cast<GLsizei>(desc.width),
                         static_cast<GLsizei>(desc.height),
                         0,
                         format,
                         GL_UNSIGNED_BYTE,
                         data);

            if (!data)
            {
                const size_t channels = (format == GL_RGBA) ? 4 : 3;
                std::vector<uint8_t> zeros(static_cast<size_t>(desc.width) *
                                               static_cast<size_t>(desc.height) *
                                               channels,
                                           0);
                glTexSubImage2D(GL_TEXTURE_2D,
                                0,
                                0,
                                0,
                                static_cast<GLsizei>(desc.width),
                                static_cast<GLsizei>(desc.height),
                                format,
                                GL_UNSIGNED_BYTE,
                                zeros.data());
            }
        }
        else if (target == GL_TEXTURE_CUBE_MAP)
        {
            for (int face = 0; face < 6; ++face)
            {
                glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face,
                             0,
                             internal,
                             static_cast<GLsizei>(desc.width),
                             static_cast<GLsizei>(desc.height),
                             0,
                             format,
                             GL_UNSIGNED_BYTE,
                             nullptr);
            }
        }
        else if (target == GL_TEXTURE_2D_ARRAY)
        {
            glTexImage3D(GL_TEXTURE_2D_ARRAY,
                         0,
                         internal,
                         static_cast<GLsizei>(desc.width),
                         static_cast<GLsizei>(desc.height),
                         static_cast<GLsizei>(desc.layers),
                         0,
                         format,
                         GL_UNSIGNED_BYTE,
                         data);
        }

        glBindTexture(target, 0);
        return std::make_shared<GLTexture>(static_cast<uint32_t>(tex), desc);
    }
} // namespace Hybrid
