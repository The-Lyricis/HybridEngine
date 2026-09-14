#pragma once

#include <array>
#include <vector>

#include "runtime/modules/render/rhi/rhi_handles.h"
#include "runtime/modules/render/runtime/pipeline/render_context.h"

namespace Hybrid
{
    class ShadowPass
    {
    public:
        ~ShadowPass();
        void execute(RenderContext& context);
        void shutdown();

    private:
        struct DrawBuffers { BufferHandle draw; BufferHandle material; };
        bool ensurePipeline(IRenderDevice& device, ShaderLibrary& shaders);
        bool ensureBuffers(IRenderDevice& device, size_t count);

        IRenderDevice* m_device = nullptr;
        ShaderHandle m_vertex_shader;
        ShaderHandle m_fragment_shader;
        PipelineHandle m_pipeline;
        std::array<BufferHandle, kMaxDirectionalShadowCascades> m_view_buffers{};
        std::vector<DrawBuffers> m_draw_buffers;
        uint64_t m_shader_revision = 0;
    };
} // namespace Hybrid
