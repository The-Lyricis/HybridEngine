#include "texture.h"
#include "renderer_api.h"
#include "opengl/opengl_texture.h"

namespace Hybrid
{
    TexturePtr Texture::Create(const TextureDesc& desc, const void* initialData, size_t bytes)
    {
        switch (RendererAPI::getAPI())
        {
        case RendererAPI::API::OpenGL:
            return GLTexture::Create(desc, initialData, bytes);
        default:
            return nullptr;
        }
    }
} // namespace Hybrid

