#pragma once

#include "runtime/modules/render/runtime/pipeline/render_context.h"
#include "runtime/modules/render/rhi/rhi_handles.h"

namespace Hybrid
{
    class IRenderDevice;
    class ShaderLibrary;

    // Fullscreen selection compositing. Its intermediate image is a graph
    // resource; this class owns only immutable pipeline state and buffers.
    class SelectionOverlayPass
    {
    public:
        ~SelectionOverlayPass();
        SelectionOverlayPass() = default;
        SelectionOverlayPass(const SelectionOverlayPass&) = delete;
        SelectionOverlayPass& operator=(const SelectionOverlayPass&) = delete;

        void execute(RenderContext& context);
        void shutdown();

    private:
        bool ensureStaticResources(IRenderDevice& device);
        bool ensurePipeline(IRenderDevice& device, ShaderLibrary& shaders);
        void releasePipeline();

        IRenderDevice* m_Device = nullptr;
        BufferHandle m_VertexBuffer;
        BufferHandle m_IndexBuffer;
        BufferHandle m_SettingsBuffer;
        SamplerHandle m_Sampler;
        ShaderHandle m_VertexShader;
        ShaderHandle m_FragmentShader;
        PipelineHandle m_Pipeline;
        uint64_t m_ShaderRevision = 0;
    };
} // namespace Hybrid
