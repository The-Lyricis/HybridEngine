#include "render_command.h"
#include <glad/gl.h>

namespace Hybrid {

    void RenderCommand::Init() {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glEnable(GL_DEPTH_TEST); // M1 画三角形不一定需要，但建议开着，后面直接用
    }

    void RenderCommand::SetViewport(int x, int y, int width, int height) {
        glViewport(x, y, width, height);
    }

    void RenderCommand::SetClearColor(const glm::vec4& color) {
        glClearColor(color.r, color.g, color.b, color.a);
    }

    void RenderCommand::Clear() {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    void RenderCommand::DrawIndexed(unsigned int indexCount) {
        glDrawElements(GL_TRIANGLES, (GLsizei)indexCount, GL_UNSIGNED_INT, nullptr);
    }

} // namespace Hybrid
