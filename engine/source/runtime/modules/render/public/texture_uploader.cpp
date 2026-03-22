#include "texture_uploader.h"

#include "runtime/modules/render/backend/opengl/opengl_texture_uploader.h"
#include "runtime/modules/render/public/renderer_api.h"

namespace Hybrid
{
    std::unique_ptr<TextureUploader> TextureUploader::Create()
    {
        switch (RendererAPI::getAPI())
        {
        case RendererAPI::API::OpenGL:
            return std::make_unique<OpenGLTextureUploader>();
        default:
            return nullptr;
        }
    }
} // namespace Hybrid
