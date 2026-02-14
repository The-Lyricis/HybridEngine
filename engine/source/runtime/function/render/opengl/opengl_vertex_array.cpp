#include "opengl_vertex_array.h"
#include "opengl_buffer.h"
#include <glad/gl.h>
#include <cstddef>

namespace Hybrid {

    GLVertexArray::GLVertexArray() {
        glGenVertexArrays(1, &m_RendererID);
    }

    GLVertexArray::~GLVertexArray() {
        glDeleteVertexArrays(1, &m_RendererID);
    }

    void GLVertexArray::bind() const {
        glBindVertexArray(m_RendererID);
    }

    void GLVertexArray::unbind() const {
        glBindVertexArray(0);
    }

    void GLVertexArray::setVertexBuffer(std::shared_ptr<VertexBuffer> vb) {
        // Fixed layout: position.xyz + color.rgba (float)
        m_VertexBuffer = vb;
        const uint32_t stride = (3 + 4) * sizeof(float);

        bind();
        vb->bind();

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (const void*)0);

        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, stride, (const void*)(3 * sizeof(float)));
    }

    void GLVertexArray::setIndexBuffer(std::shared_ptr<IndexBuffer> ib) {
        m_IndexBuffer = ib;
        bind();
        ib->bind();
    }

    uint32_t GLVertexArray::getIndexCount() const {
        return m_IndexBuffer ? m_IndexBuffer->getCount() : 0;
    }

} // namespace Hybrid
