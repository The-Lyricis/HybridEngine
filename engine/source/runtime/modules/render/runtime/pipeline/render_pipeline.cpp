#include "render_pipeline.h"

namespace Hybrid
{
    RenderPipeline::RenderPipeline()
        : m_compiled_graph(CompileRenderGraph(CreateDefaultRenderGraphBuild()))
    {
    }

    void RenderPipeline::execute(RenderContext& context, const RenderPipelineCallbacks& callbacks) const
    {
        for (const CompiledRenderGraphPass& compiled_pass : m_compiled_graph.passes)
        {
            const RenderGraphPassDesc& pass_desc = compiled_pass.desc;
            RenderPassType pass = pass_desc.type;
            if (!shouldRun(pass, context.flags))
                continue;
            invoke(pass, context, callbacks);
        }
    }

    bool RenderPipeline::shouldRun(RenderPassType pass, RenderFlags flags) const
    {
        switch (pass)
        {
        case RenderPassType::Scene:
            return HasFlag(flags, RenderFlags::Scene) ||
                   HasFlag(flags, RenderFlags::PickingID) ||
                   HasFlag(flags, RenderFlags::SelectionHighlight);
        case RenderPassType::Skybox:
            return HasFlag(flags, RenderFlags::Scene) ||
                   HasFlag(flags, RenderFlags::PickingID) ||
                   HasFlag(flags, RenderFlags::SelectionHighlight);
        case RenderPassType::Picking:
            return HasFlag(flags, RenderFlags::PickingID);
        case RenderPassType::SelectionMask:
        case RenderPassType::SelectionOverlay:
            return HasFlag(flags, RenderFlags::SelectionHighlight);
        case RenderPassType::WorldGizmo:
        case RenderPassType::OverlayGizmo:
            return HasFlag(flags, RenderFlags::Gizmo);
        case RenderPassType::Grid:
            return HasFlag(flags, RenderFlags::Grid);
        case RenderPassType::Shadow:
            return HasFlag(flags, RenderFlags::Shadow);
        case RenderPassType::PostProcess:
            return HasFlag(flags, RenderFlags::PostProcess);
        // case RenderPassType::DebugNormals:
        //     return HasFlag(flags, RenderFlags::DebugNormals);
        default:
            return false;
        }
    }

    void RenderPipeline::invoke(RenderPassType pass, RenderContext& context, const RenderPipelineCallbacks& callbacks) const
    {
        switch (pass)
        {
        case RenderPassType::Scene:
            if (callbacks.scene)
                callbacks.scene(context);
            break;
        case RenderPassType::Skybox:
            if (callbacks.skybox)
                callbacks.skybox(context);
            break;
        case RenderPassType::Picking:
            if (callbacks.picking)
                callbacks.picking(context);
            break;
        case RenderPassType::SelectionMask:
            if (callbacks.selection_mask)
                callbacks.selection_mask(context);
            break;
        case RenderPassType::SelectionOverlay:
            if (callbacks.selection_overlay)
                callbacks.selection_overlay(context);
            break;
        case RenderPassType::WorldGizmo:
            if (callbacks.world_gizmo)
                callbacks.world_gizmo(context);
            break;
        case RenderPassType::OverlayGizmo:
            if (callbacks.overlay_gizmo)
                callbacks.overlay_gizmo(context);
            break;
        case RenderPassType::Grid:
            if (callbacks.grid)
                callbacks.grid(context);
            break;
        case RenderPassType::Shadow:
            if (callbacks.shadow)
                callbacks.shadow(context);
            break;
        case RenderPassType::PostProcess:
            if (callbacks.post_process)
                callbacks.post_process(context);
            break;
        // case RenderPassType::DebugNormals:
        //     if (callbacks.debug_normals)
        //         callbacks.debug_normals(context);
        //     break;
        default:
            break;
        }
    }

    std::string RenderPipeline::describeGraph() const
    {
        std::vector<RenderGraphPassDesc> pass_descs;
        pass_descs.reserve(m_compiled_graph.passes.size());
        for (const CompiledRenderGraphPass& compiled_pass : m_compiled_graph.passes)
        {
            pass_descs.push_back(compiled_pass.desc);
        }

        return DescribeRenderGraph(pass_descs, m_compiled_graph.resources);
    }
} // namespace Hybrid
