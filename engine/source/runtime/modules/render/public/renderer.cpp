#include "renderer.h"
#include "render_command.h"
#include "shader.h"

namespace Hybrid {

    void Renderer::initialize() {
        RenderCommand::initialize();
    }

    void Renderer::shutdown() {
        RenderCommand::shutdown();
    }

    void Renderer::beginFrame(const glm::vec4& clearColor) {
        RenderCommand::setClearColor(clearColor);
        RenderCommand::clear();
    }

    void Renderer::submit(const std::shared_ptr<VertexArray>& va, const std::shared_ptr<Shader>& shader) {
        shader->bind();
        va->bind();
        RenderCommand::drawIndexed(va->getIndexCount());
    }

    void Renderer::endFrame() {
        // M1 空实现；后续可统计 drawcall/提交 GPU 命令等
    }

} // namespace Hybrid
