#pragma once

#include <memory>

namespace Hybrid
{
    struct TextureImageData;
    class Texture;
    using TexturePtr = std::shared_ptr<Texture>;

    class TextureUploader
    {
    public:
        virtual ~TextureUploader() = default;

        virtual TexturePtr uploadTexture2D(const TextureImageData& image) = 0;

        static std::unique_ptr<TextureUploader> Create();
    };
} // namespace Hybrid
