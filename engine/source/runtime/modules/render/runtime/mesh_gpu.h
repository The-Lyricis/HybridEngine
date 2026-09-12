#pragma once

#include <memory>
#include <vector>

#include "runtime/modules/asset/mesh.h"
#include "runtime/modules/render/rhi/rhi_handles.h"

namespace Hybrid
{
    class VertexArray;
    class VertexBuffer;
    class IndexBuffer;

    struct MeshGPU
    {
        // Legacy objects remain only until Scene/Shadow/SelectionMask consume
        // the RHI handles below. New passes must use the RHI representation.
        std::shared_ptr<VertexArray> vao;
        std::shared_ptr<VertexBuffer> vb;
        std::shared_ptr<IndexBuffer> ib;
        BufferHandle rhi_vertex_buffer;
        BufferHandle rhi_index_buffer;
        std::vector<Submesh> submeshes;
    };
} // namespace Hybrid
