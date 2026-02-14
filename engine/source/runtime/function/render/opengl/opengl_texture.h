#pragma once
#include "../texture.h"
#include <glad/gl.h>

namespace Hybrid {

class GLTexture final : public Texture {
public:
    GLTexture(uint32_t id, const TextureDesc& desc)
        : m_id(id), m_desc(desc) {}

    ~GLTexture() override {
        if (m_id) glDeleteTextures(1, &m_id);
    }

    const TextureDesc& getDesc() const override { return m_desc; }
    uint32_t getWidth()  const override { return m_desc.width; }
    uint32_t getHeight() const override { return m_desc.height; }
    TextureFormat getFormat() const override { return m_desc.format; }

    void bind(uint32_t slot = 0) const override {
        glBindTextureUnit(slot, m_id);
    }

    uint32_t id() const { return m_id; }

private:
    uint32_t    m_id{0};
    TextureDesc m_desc{};
};

} // namespace Hybrid
