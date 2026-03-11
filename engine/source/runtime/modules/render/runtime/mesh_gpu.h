#pragma once

#include <memory>
#include <vector>

#include "runtime/modules/asset/mesh.h"

namespace Hybrid
{
    class VertexArray;
    class VertexBuffer;
    class IndexBuffer;

    struct MeshGPU
    {
        std::shared_ptr<VertexArray> vao;
        std::shared_ptr<VertexBuffer> vb;
        std::shared_ptr<IndexBuffer> ib;
        std::vector<Submesh> submeshes;
    };
} // namespace Hybrid
