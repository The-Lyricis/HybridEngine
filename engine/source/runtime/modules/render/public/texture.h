#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>

namespace Hybrid
{
    class Texture;
    using TexturePtr = std::shared_ptr<Texture>;

    enum class TextureFormat
    {
        Unknown = 0,
        RGB8,
        RGBA8,
    };

    enum class TextureType : uint8_t
    {
        Tex2D = 0,
        Cube,
        Tex2DArray,
    };

    struct TextureDesc
    {
        TextureType type = TextureType::Tex2D;
        TextureFormat format = TextureFormat::RGBA8;
        uint32_t width = 1;
        uint32_t height = 1;
        uint32_t layers = 1;
        uint32_t mipLevels = 1;
        bool srgb = false;
    };

    class Texture
    {
    public:
        virtual ~Texture() = default;

        static TexturePtr Create(const TextureDesc& desc,
                                 const void* initialData = nullptr,
                                 size_t bytes = 0);

        virtual const TextureDesc& getDesc() const = 0;
        virtual uint32_t getWidth() const = 0;
        virtual uint32_t getHeight() const = 0;
        virtual TextureFormat getFormat() const = 0;

        virtual void bind(uint32_t slot = 0) const = 0;
    };
} // namespace Hybrid
