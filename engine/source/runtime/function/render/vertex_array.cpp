#include "vertex_array.h"
#include <glad/gl.h>

namespace Hybrid {

    VertexArray::VertexArray() {
        glGenVertexArrays(1, &m_RendererID);
    }

    VertexArray::~VertexArray() {
        glDeleteVertexArrays(1, &m_RendererID);
    }

    void VertexArray::Bind() const {
        glBindVertexArray(m_RendererID);
    }

    void VertexArray::Unbind() const {
        glBindVertexArray(0);
    }

    void VertexArray::SetVertexBuffer(std::shared_ptr<VertexBuffer> vb) {
        m_VertexBuffer = vb;
        Bind();
        vb->Bind();

        // 布局：pos(vec3) + color(vec4)
        // stride = 7 floats
        const GLsizei stride = 7 * sizeof(float);

        // location 0: position
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (const void*)0);

        // location 1: color
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, stride, (const void*)(3 * sizeof(float)));
    }

    void VertexArray::SetIndexBuffer(std::shared_ptr<IndexBuffer> ib) {
        m_IndexBuffer = ib;
        Bind();
        ib->Bind(); // 注意：EBO 绑定是 VAO 状态的一部分
    }

    uint32_t VertexArray::GetIndexCount() const {
        return m_IndexBuffer ? m_IndexBuffer->GetCount() : 0;
    }

} // namespace Hybrid
