#pragma once
#include "../texture.h"
#include <glad/gl.h>
#include <vector>

namespace Hybrid {

    class GLTexture final : public Texture {
    public:

        static TexturePtr Create(const TextureDesc& desc,
            const void* data = nullptr,
            std::size_t bytes = 0);

        GLTexture(uint32_t id, const TextureDesc& desc)
            : m_id(id), m_desc(desc)
        {
            // 记录 target，bind 时才能正确绑定 cube/array
            switch (desc.type)
            {
            case TextureType::Tex2D:      m_target = GL_TEXTURE_2D; break;
            case TextureType::Cube:       m_target = GL_TEXTURE_CUBE_MAP; break;
            case TextureType::Tex2DArray: m_target = GL_TEXTURE_2D_ARRAY; break;
            default:                      m_target = GL_TEXTURE_2D; break;
            }
        }

        ~GLTexture() override {
            if (m_id) glDeleteTextures(1, &m_id);
        }

        const TextureDesc& getDesc() const override { return m_desc; }
        uint32_t getWidth()  const override { return m_desc.width; }
        uint32_t getHeight() const override { return m_desc.height; }
        TextureFormat getFormat() const override { return m_desc.format; }

        void bind(uint32_t slot = 0) const override
        {
            // ⚠ 若你的 OpenGL 是 3.3：glBindTextureUnit 可能不可用
            glActiveTexture(GL_TEXTURE0 + slot);
            glBindTexture(m_target, m_id);
        }

        uint32_t id() const { return m_id; }

    private:
        uint32_t    m_id{ 0 };
        TextureDesc m_desc{};
        GLenum      m_target{ GL_TEXTURE_2D };
    };

    inline TexturePtr GLTexture::Create(const TextureDesc& desc, const void* data, std::size_t bytes)
    {
        GLuint tex = 0;
        GLenum target = GL_TEXTURE_2D;
        switch (desc.type)
        {
        case TextureType::Tex2D:      target = GL_TEXTURE_2D; break;
        case TextureType::Cube:       target = GL_TEXTURE_CUBE_MAP; break;
        case TextureType::Tex2DArray: target = GL_TEXTURE_2D_ARRAY; break;
        default: return nullptr;
        }

        glGenTextures(1, &tex);
        glBindTexture(target, tex);

        glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_REPEAT);

        if (target == GL_TEXTURE_2D)
        {
            const GLenum format = (desc.format == TextureFormat::RGBA8) ? GL_RGBA : GL_RGB;
            const GLenum internal = (desc.format == TextureFormat::RGBA8) ? GL_RGBA8 : GL_RGB8;

            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            glTexImage2D(GL_TEXTURE_2D, 0, internal,
                (GLsizei)desc.width, (GLsizei)desc.height,
                0, format, GL_UNSIGNED_BYTE, data);

            // 无初始数据则填 0
            if (!data)
            {
                const size_t channels = (format == GL_RGBA) ? 4 : 3;
                std::vector<uint8_t> zeros((size_t)desc.width * (size_t)desc.height * channels, 0);
                glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
                    (GLsizei)desc.width, (GLsizei)desc.height,
                    format, GL_UNSIGNED_BYTE, zeros.data());
            }
        }
        else if (target == GL_TEXTURE_CUBE_MAP)
        {
            //TODO: 实现 Cube
        }
        else if (target == GL_TEXTURE_2D_ARRAY)
        {
            //TODO: 实现 Array
        }

        glBindTexture(target, 0);
        return std::make_shared<GLTexture>((uint32_t)tex, desc);
    }

} // namespace Hybrid
