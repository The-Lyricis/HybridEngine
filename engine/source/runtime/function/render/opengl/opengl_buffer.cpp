#include "opengl_buffer.h"
#include <glad/gl.h>

namespace Hybrid {

    GLVertexBuffer::GLVertexBuffer(const void* data, uint32_t size) {
        glGenBuffers(1, &m_RendererID);
        glBindBuffer(GL_ARRAY_BUFFER, m_RendererID);
        glBufferData(GL_ARRAY_BUFFER, size, data, GL_STATIC_DRAW);
    }

    GLVertexBuffer::~GLVertexBuffer() {
        glDeleteBuffers(1, &m_RendererID);
    }

    void GLVertexBuffer::bind() const {
        glBindBuffer(GL_ARRAY_BUFFER, m_RendererID);
    }

    void GLVertexBuffer::unbind() const {
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    void GLVertexBuffer::setData(const void* data, uint32_t size) {
        glBindBuffer(GL_ARRAY_BUFFER, m_RendererID);
        glBufferSubData(GL_ARRAY_BUFFER, 0, size, data);
    }

    GLIndexBuffer::GLIndexBuffer(const uint32_t* indices, uint32_t count)
        : m_Count(count) {
        glGenBuffers(1, &m_RendererID);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_RendererID);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, count * sizeof(uint32_t), indices, GL_STATIC_DRAW);
    }

    GLIndexBuffer::~GLIndexBuffer() {
        glDeleteBuffers(1, &m_RendererID);
    }

    void GLIndexBuffer::bind() const {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_RendererID);
    }

    void GLIndexBuffer::unbind() const {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    }

} // namespace Hybrid
