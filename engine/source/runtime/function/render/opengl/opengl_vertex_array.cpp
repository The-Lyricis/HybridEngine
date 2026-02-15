#include "opengl_vertex_array.h"
#include "opengl_buffer.h"
#include <glad/gl.h>
#include <cstddef>
#include <cassert>

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

    void GLVertexArray::setVertexBuffer(std::shared_ptr<VertexBuffer> vb, const VertexLayout& layout) {
        m_VertexBuffer = vb;

        bind();
        vb->bind();

        assert(layout.stride > 0);
        for (const auto& attr : layout.attributes) {
            glEnableVertexAttribArray(attr.index);
            glVertexAttribPointer(
                attr.index,
                attr.count,
                GL_FLOAT,
                attr.normalized ? GL_TRUE : GL_FALSE,
                layout.stride,
                reinterpret_cast<const void*>(static_cast<uintptr_t>(attr.offset))
            );
        }
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
