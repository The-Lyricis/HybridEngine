#include "pipeline_state.h"

#include "runtime/modules/render/public/render_command.h"

namespace Hybrid
{
    namespace
    {
        PipelineStateDesc g_CurrentPipelineState = PipelineStates::OpaqueDepth();
    }

    namespace PipelineStates
    {
        PipelineStateDesc OpaqueDepth()
        {
            PipelineStateDesc desc{};
            desc.blend_enabled = false;
            desc.depth_test_enabled = true;
            desc.depth_write_enabled = true;
            desc.depth_compare = DepthCompareFunc::Less;
            desc.cull_mode = CullMode::Back;
            desc.topology = PrimitiveTopology::Triangles;
            desc.line_width = 1.0f;
            desc.debug_name = "OpaqueDepth";
            return desc;
        }

        PipelineStateDesc TransparentDepthRead()
        {
            PipelineStateDesc desc = OpaqueDepth();
            desc.blend_enabled = true;
            desc.depth_write_enabled = false;
            desc.debug_name = "TransparentDepthRead";
            return desc;
        }

        PipelineStateDesc DepthOnly()
        {
            PipelineStateDesc desc = OpaqueDepth();
            desc.blend_enabled = false;
            desc.debug_name = "DepthOnly";
            return desc;
        }

        PipelineStateDesc FullscreenNoDepth()
        {
            PipelineStateDesc desc{};
            desc.blend_enabled = false;
            desc.depth_test_enabled = false;
            desc.depth_write_enabled = false;
            desc.depth_compare = DepthCompareFunc::Less;
            desc.cull_mode = CullMode::None;
            desc.topology = PrimitiveTopology::Triangles;
            desc.line_width = 1.0f;
            desc.debug_name = "FullscreenNoDepth";
            return desc;
        }

        PipelineStateDesc WorldOverlayLines(float line_width)
        {
            PipelineStateDesc desc{};
            desc.blend_enabled = false;
            desc.depth_test_enabled = true;
            desc.depth_write_enabled = true;
            desc.depth_compare = DepthCompareFunc::Less;
            desc.cull_mode = CullMode::None;
            desc.topology = PrimitiveTopology::Lines;
            desc.line_width = line_width;
            desc.debug_name = "WorldOverlayLines";
            return desc;
        }

        PipelineStateDesc HighlightOverlayLines(float line_width)
        {
            PipelineStateDesc desc = WorldOverlayLines(line_width);
            desc.debug_name = "HighlightOverlayLines";
            return desc;
        }

        PipelineStateDesc DebugOverlayLines(float line_width)
        {
            PipelineStateDesc desc = WorldOverlayLines(line_width);
            desc.debug_name = "DebugOverlayLines";
            return desc;
        }

        PipelineStateDesc Skybox()
        {
            PipelineStateDesc desc{};
            desc.blend_enabled = false;
            desc.depth_test_enabled = true;
            desc.depth_write_enabled = false;
            desc.depth_compare = DepthCompareFunc::LessEqual;
            desc.cull_mode = CullMode::None;
            desc.topology = PrimitiveTopology::Triangles;
            desc.line_width = 1.0f;
            desc.debug_name = "Skybox";
            return desc;
        }
    } // namespace PipelineStates

    const PipelineStateDesc& GetCurrentPipelineState()
    {
        return g_CurrentPipelineState;
    }

    void ApplyPipelineState(const PipelineStateDesc& desc)
    {
        RenderCommand::setBlendEnabled(desc.blend_enabled);
        RenderCommand::setDepthTestEnabled(desc.depth_test_enabled);
        RenderCommand::setDepthWriteEnabled(desc.depth_write_enabled);
        RenderCommand::setDepthCompareFunc(desc.depth_compare);
        RenderCommand::setCullEnabled(desc.cull_mode != CullMode::None);
        RenderCommand::setLineWidth(desc.topology == PrimitiveTopology::Lines ? desc.line_width : 1.0f);
        g_CurrentPipelineState = desc;
    }

    ScopedPipelineState::ScopedPipelineState(const PipelineStateDesc& desc)
        : m_Previous(GetCurrentPipelineState())
    {
        ApplyPipelineState(desc);
    }

    ScopedPipelineState::~ScopedPipelineState()
    {
        ApplyPipelineState(m_Previous);
    }
} // namespace Hybrid
