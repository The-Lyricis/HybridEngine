#include "renderer.h"
#include "render_command.h"
#include "shader.h"

namespace Hybrid {

    void Renderer::Init() {
        RenderCommand::Init();
    }

    void Renderer::BeginFrame(const glm::vec4& clearColor) {
        RenderCommand::SetClearColor(clearColor);
        RenderCommand::Clear();
    }

    void Renderer::Submit(const std::shared_ptr<VertexArray>& va, const std::shared_ptr<Shader>& shader) {
        shader->Bind();
        va->Bind();
        RenderCommand::DrawIndexed(va->GetIndexCount());
    }

    void Renderer::EndFrame() {
        // M1 空实现；后续可统计 drawcall/提交 GPU 命令等
    }

} // namespace Hybrid
