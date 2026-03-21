#include "opengl_renderer_api.h"
#include <glad/gl.h>

namespace Hybrid {

    void GLRendererAPI::init() {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glEnable(GL_DEPTH_TEST);
    }

    void GLRendererAPI::setViewport(int x, int y, int width, int height) {
        glViewport(x, y, width, height);
    }

    void GLRendererAPI::setClearColor(const glm::vec4& color) {
        glClearColor(color.r, color.g, color.b, color.a);
    }

    void GLRendererAPI::setClearDepth(float depth) {
        glClearDepth(depth);
    }

    void GLRendererAPI::clear() {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    void GLRendererAPI::setBlendEnabled(bool enabled) {
        if (enabled) glEnable(GL_BLEND);
        else glDisable(GL_BLEND);
    }

    void GLRendererAPI::setDepthTestEnabled(bool enabled) {
        if (enabled) glEnable(GL_DEPTH_TEST);
        else glDisable(GL_DEPTH_TEST);
    }

    void GLRendererAPI::setCullEnabled(bool enabled) {
        if (enabled) glEnable(GL_CULL_FACE);
        else glDisable(GL_CULL_FACE);
    }

    void GLRendererAPI::setDepthWriteEnabled(bool enabled) {
        glDepthMask(enabled ? GL_TRUE : GL_FALSE);
    }

    void GLRendererAPI::setLineWidth(float width) {
        glLineWidth(width);
    }

    void GLRendererAPI::drawIndexed(uint32_t indexCount, uint32_t indexOffset) {
        const void* offsetPtr = reinterpret_cast<const void*>(static_cast<uintptr_t>(indexOffset) * sizeof(uint32_t));
        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indexCount), GL_UNSIGNED_INT, offsetPtr);
    }

    void GLRendererAPI::drawLinesIndexed(uint32_t indexCount, uint32_t indexOffset) {
        const void* offsetPtr = reinterpret_cast<const void*>(static_cast<uintptr_t>(indexOffset) * sizeof(uint32_t));
        glDrawElements(GL_LINES, static_cast<GLsizei>(indexCount), GL_UNSIGNED_INT, offsetPtr);
    }

} // namespace Hybrid
