#include "shader.h"
#include "renderer_api.h"
#include "runtime/modules/render/backend/opengl/opengl_shader.h"

namespace Hybrid {

    std::shared_ptr<Shader> Shader::Create(const std::string& vsSource, const std::string& fsSource) {
        switch (RendererAPI::getAPI()) {
        case RendererAPI::API::OpenGL:
            return std::make_shared<GLShader>(vsSource, fsSource);
        default:
            return nullptr;
        }
    }

} // namespace Hybrid


