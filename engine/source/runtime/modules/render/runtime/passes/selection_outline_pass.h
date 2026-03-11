#pragma once

#include <memory>

#include "runtime/modules/render/runtime/render_context.h"

namespace Hybrid
{
    class Framebuffer;
    class VertexArray;
    class VertexBuffer;
    class IndexBuffer;

    class SelectionOutlinePass
    {
    public:
        void execute(RenderContext& context);

    private:
        struct FullscreenQuadGPU
        {
            std::shared_ptr<VertexArray> vao;
            std::shared_ptr<VertexBuffer> vb;
            std::shared_ptr<IndexBuffer> ib;
            uint32_t index_count = 0;
        };

        void ensureInputFramebuffer(uint32_t width, uint32_t height);
        FullscreenQuadGPU* getOrCreateFullscreenQuad();

    private:
        std::shared_ptr<Framebuffer> m_InputFramebuffer;
        FullscreenQuadGPU m_FullscreenQuad;
        bool m_HasFullscreenQuad = false;
    };
} // namespace Hybrid
