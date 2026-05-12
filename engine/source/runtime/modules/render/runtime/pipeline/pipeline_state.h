#pragma once

#include "runtime/modules/render/public/renderer_api.h"

namespace Hybrid
{
    enum class PrimitiveTopology : unsigned char
    {
        Triangles,
        Lines
    };

    enum class CullMode : unsigned char
    {
        None,
        Back
    };

    struct PipelineStateDesc
    {
        bool blend_enabled = false;
        bool depth_test_enabled = true;
        bool depth_write_enabled = true;
        DepthCompareFunc depth_compare = DepthCompareFunc::Less;
        CullMode cull_mode = CullMode::Back;
        PrimitiveTopology topology = PrimitiveTopology::Triangles;
        float line_width = 1.0f;
        const char* debug_name = "UnnamedPipelineState";
    };

    namespace PipelineStates
    {
        PipelineStateDesc OpaqueDepth();
        PipelineStateDesc TransparentDepthRead();
        PipelineStateDesc DepthOnly();
        PipelineStateDesc FullscreenNoDepth();
        PipelineStateDesc WorldOverlayLines(float line_width = 2.0f);
        PipelineStateDesc HighlightOverlayLines(float line_width = 3.0f);
        PipelineStateDesc DebugOverlayLines(float line_width = 1.5f);
        PipelineStateDesc Skybox();
    }

    const PipelineStateDesc& GetCurrentPipelineState();
    void ApplyPipelineState(const PipelineStateDesc& desc);

    class ScopedPipelineState
    {
    public:
        explicit ScopedPipelineState(const PipelineStateDesc& desc);
        ~ScopedPipelineState();

        ScopedPipelineState(const ScopedPipelineState&) = delete;
        ScopedPipelineState& operator=(const ScopedPipelineState&) = delete;

    private:
        PipelineStateDesc m_Previous;
    };
} // namespace Hybrid
