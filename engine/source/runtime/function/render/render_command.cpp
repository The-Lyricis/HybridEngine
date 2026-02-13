#include "render_command.h"

namespace Hybrid {

    std::unique_ptr<RendererAPI> RenderCommand::s_API;

    void RenderCommand::initialize() {
        s_API = RendererAPI::Create();
        s_API->init();
    }

    void RenderCommand::setViewport(int x, int y, int width, int height) {
        s_API->setViewport(x, y, width, height);
    }

    void RenderCommand::setClearColor(const glm::vec4& color) {
        s_API->setClearColor(color);
    }

    void RenderCommand::clear() {
        s_API->clear();
    }

    void RenderCommand::drawIndexed(unsigned int indexCount) {
        s_API->drawIndexed(indexCount);
    }

} // namespace Hybrid
