#pragma once

#include "runtime/modules/render/runtime/pipeline/render_context.h"
#include "runtime/modules/render/rhi/rhi_handles.h"

namespace Hybrid
{
    class IRenderDevice;
    class ShaderLibrary;

    class PostProcessPass
    {
    public:
        ~PostProcessPass();

        PostProcessPass() = default;
        PostProcessPass(const PostProcessPass&) = delete;
        PostProcessPass& operator=(const PostProcessPass&) = delete;

        struct Settings
        {
            bool enable_tone_mapping = false;
            bool enable_gamma_correction = false;
            float exposure = 1.0f;
            float gamma = 2.2f;
        };

        void execute(RenderContext& context);
        void shutdown();
        void setSettings(const Settings& settings) { m_Settings = settings; }
        const Settings& getSettings() const { return m_Settings; }

    private:
        bool ensurePipeline(IRenderDevice& device, ShaderLibrary& shaders);
        bool ensureStaticResources(IRenderDevice& device);
        void releasePipeline();

    private:
        IRenderDevice* m_Device = nullptr;
        BufferHandle m_VertexBuffer;
        BufferHandle m_IndexBuffer;
        BufferHandle m_SettingsBuffer;
        SamplerHandle m_Sampler;
        ShaderHandle m_VertexShader;
        ShaderHandle m_FragmentShader;
        PipelineHandle m_Pipeline;
        Settings m_Settings{};
        uint64_t m_ShaderRevision = 0;
    };
} // namespace Hybrid
