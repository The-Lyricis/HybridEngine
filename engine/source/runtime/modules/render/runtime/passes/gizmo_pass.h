#pragma once

#include <memory>

#include "runtime/modules/render/runtime/render_context.h"

namespace Hybrid
{
    class VertexArray;
    class VertexBuffer;
    class IndexBuffer;

    class GizmoPass
    {
    public:
        void execute(RenderContext& context);

    private:
        struct DebugLineMeshGPU
        {
            std::shared_ptr<VertexArray> vao;
            std::shared_ptr<VertexBuffer> vb;
            std::shared_ptr<IndexBuffer> ib;
            uint32_t index_count = 0;
        };

        DebugLineMeshGPU* getOrCreateDebugBoxMeshGPU();

    private:
        DebugLineMeshGPU m_DebugBoxMeshGPU;
        bool m_HasDebugBoxMeshGPU = false;
    };
} // namespace Hybrid
