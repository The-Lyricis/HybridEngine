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

    void RenderCommand::setClearDepth(float depth) {
        s_API->setClearDepth(depth);
    }

    void RenderCommand::clear() {
        s_API->clear();
    }

    void RenderCommand::setBlendEnabled(bool enabled) {
        s_API->setBlendEnabled(enabled);
    }

    void RenderCommand::setDepthTestEnabled(bool enabled) {
        s_API->setDepthTestEnabled(enabled);
    }

    void RenderCommand::setCullEnabled(bool enabled) {
        s_API->setCullEnabled(enabled);
    }

    void RenderCommand::setDepthWriteEnabled(bool enabled) {
        s_API->setDepthWriteEnabled(enabled);
    }

    void RenderCommand::setLineWidth(float width) {
        s_API->setLineWidth(width);
    }

    void RenderCommand::drawIndexed(unsigned int indexCount, unsigned int indexOffset) {
        s_API->drawIndexed(indexCount, indexOffset);
    }

    void RenderCommand::drawLinesIndexed(unsigned int indexCount, unsigned int indexOffset) {
        s_API->drawLinesIndexed(indexCount, indexOffset);
    }

} // namespace Hybrid
