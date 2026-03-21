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
        : m_Count(count)
    {
        glCreateBuffers(1, &m_RendererID);
        glNamedBufferData(
            m_RendererID,
            static_cast<GLsizeiptr>(count * sizeof(uint32_t)),
            indices,
            GL_STATIC_DRAW
        );
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

    GLUniformBuffer::GLUniformBuffer(uint32_t size)
        : m_Size(size)
    {
        glCreateBuffers(1, &m_RendererID);
        glNamedBufferData(m_RendererID, static_cast<GLsizeiptr>(size), nullptr, GL_DYNAMIC_DRAW);
    }

    GLUniformBuffer::~GLUniformBuffer() {
        glDeleteBuffers(1, &m_RendererID);
    }

    void GLUniformBuffer::setData(const void* data, uint32_t size, uint32_t offset) {
        glNamedBufferSubData(m_RendererID,
                             static_cast<GLintptr>(offset),
                             static_cast<GLsizeiptr>(size),
                             data);
    }

    void GLUniformBuffer::bindBase(uint32_t binding) const {
        glBindBufferBase(GL_UNIFORM_BUFFER, binding, m_RendererID);
    }

} // namespace Hybrid
