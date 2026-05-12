#pragma once

#include <memory>

#include "runtime/modules/render/runtime/pipeline/render_context.h"

namespace Hybrid
{
    class Framebuffer;
    class VertexArray;
    class VertexBuffer;
    class IndexBuffer;

    class PostProcessPass
    {
    public:
        struct Settings
        {
            bool enable_tone_mapping = false;
            bool enable_gamma_correction = false;
            float exposure = 1.0f;
            float gamma = 2.2f;
        };

        void execute(RenderContext& context);
        void setSettings(const Settings& settings) { m_Settings = settings; }
        const Settings& getSettings() const { return m_Settings; }

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
        Settings m_Settings{};
        bool m_HasFullscreenQuad = false;
    };
} // namespace Hybrid
