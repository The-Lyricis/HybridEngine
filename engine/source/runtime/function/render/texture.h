#pragma once
#include <cstdint>
#include <memory>

namespace Hybrid {

enum class TextureFormat { Unknown = 0, RGB8, RGBA8 };

enum class TextureType : uint8_t { Tex2D = 0, Cube, Tex2DArray };

struct TextureDesc {
    TextureType   type   = TextureType::Tex2D;
    TextureFormat format = TextureFormat::RGBA8;
    uint32_t      width  = 1;
    uint32_t      height = 1;
    uint32_t      layers = 1;   // 对于 Cube 固定 6，Array 可 >1
    uint32_t      mipLevels = 1;
};

class Texture {
public:
    virtual ~Texture() = default;

    virtual const TextureDesc& getDesc() const = 0;
    virtual uint32_t           getWidth() const  = 0;
    virtual uint32_t           getHeight() const = 0;
    virtual TextureFormat      getFormat() const = 0;

    virtual void bind(uint32_t slot = 0) const = 0;
};

using TexturePtr = std::shared_ptr<Texture>;

} // namespace Hybrid
