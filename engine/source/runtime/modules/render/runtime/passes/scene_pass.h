#pragma once

#include <cstddef>
#include <vector>

#include "runtime/modules/render/rhi/rhi_handles.h"
#include "runtime/modules/render/runtime/pipeline/render_context.h"

namespace Hybrid
{
    class IRenderDevice;
    class ShaderLibrary;

    class ScenePass
    {
    public:
        ~ScenePass();
        ScenePass() = default;
        ScenePass(const ScenePass&) = delete;
        ScenePass& operator=(const ScenePass&) = delete;

        void execute(RenderContext& context);
        void shutdown();

    private:
        struct DrawResources
        {
            BufferHandle draw_buffer;
            BufferHandle material_buffer;
        };

        bool ensurePipelines(IRenderDevice& device, ShaderLibrary& shaders);
        bool ensureDrawResources(IRenderDevice& device, size_t count);
        void releasePipelines();

        IRenderDevice* m_Device = nullptr;
        ShaderHandle m_VertexShader;
        ShaderHandle m_FragmentShader;
        PipelineHandle m_OpaquePipeline;
        PipelineHandle m_TransparentPipeline;
        SamplerHandle m_ShadowSampler;
        std::vector<DrawResources> m_DrawResources;
        uint64_t m_ShaderRevision = 0;
    };
} // namespace Hybrid
