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
        DebugLineMeshGPU* getOrCreateDebugSphereMeshGPU();
        DebugLineMeshGPU* getOrCreateShadowHullMeshGPU();
        DebugLineMeshGPU* getOrCreateDebugQuadMeshGPU();

    private:
        DebugLineMeshGPU m_DebugBoxMeshGPU;
        DebugLineMeshGPU m_DebugSphereMeshGPU;
        DebugLineMeshGPU m_ShadowHullMeshGPU;
        DebugLineMeshGPU m_DebugQuadMeshGPU;
        bool m_HasDebugBoxMeshGPU = false;
        bool m_HasDebugSphereMeshGPU = false;
        bool m_HasShadowHullMeshGPU = false;
        bool m_HasDebugQuadMeshGPU = false;
    };
} // namespace Hybrid
