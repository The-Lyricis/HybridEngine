#pragma once

#include <memory>

#include "runtime/modules/render/runtime/pipeline/render_context.h"

namespace Hybrid
{
    class VertexArray;
    class VertexBuffer;
    class IndexBuffer;

    class SkyboxPass
    {
    public:
        void execute(RenderContext& context);

    private:
        struct SkyboxCubeGPU
        {
            std::shared_ptr<VertexArray> vao;
            std::shared_ptr<VertexBuffer> vb;
            std::shared_ptr<IndexBuffer> ib;
            uint32_t index_count = 0;
        };

        SkyboxCubeGPU* getOrCreateSkyboxCube();

    private:
        SkyboxCubeGPU m_SkyboxCube;
        bool m_HasSkyboxCube = false;
    };
} // namespace Hybrid
