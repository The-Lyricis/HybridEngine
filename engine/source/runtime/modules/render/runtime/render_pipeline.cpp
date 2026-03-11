#include "render_pipeline.h"

namespace Hybrid
{
    RenderPipeline::RenderPipeline()
        : m_pass_order
        {
            RenderPassType::Forward,
            RenderPassType::Picking,
            RenderPassType::SelectionOutline,
            RenderPassType::Gizmos,
            //RenderPassType::Grid,
            RenderPassType::Shadows,
            RenderPassType::PostProcess,
            //RenderPassType::DebugNormals
        }
    {
    }

    void RenderPipeline::execute(RenderContext& context, const RenderPipelineCallbacks& callbacks) const
    {
        for (RenderPassType pass : m_pass_order)
        {
            if (!shouldRun(pass, context.flags))
                continue;
            invoke(pass, context, callbacks);
        }
    }

    bool RenderPipeline::shouldRun(RenderPassType pass, RenderFlags flags) const
    {
        switch (pass)
        {
        case RenderPassType::Forward:
            return HasFlag(flags, RenderFlags::Forward) ||
                   HasFlag(flags, RenderFlags::PickingID) ||
                   HasFlag(flags, RenderFlags::SelectionOutline);
        case RenderPassType::Picking:
            return HasFlag(flags, RenderFlags::PickingID);
        case RenderPassType::SelectionOutline:
            return HasFlag(flags, RenderFlags::SelectionOutline);
        case RenderPassType::Gizmos:
            return HasFlag(flags, RenderFlags::Gizmos);
        // case RenderPassType::Grid:
        //     return HasFlag(flags, RenderFlags::Grid);
        case RenderPassType::Shadows:
            return HasFlag(flags, RenderFlags::Shadows);
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
        case RenderPassType::Forward:
            if (callbacks.forward)
                callbacks.forward(context);
            break;
        case RenderPassType::Picking:
            if (callbacks.picking)
                callbacks.picking(context);
            break;
        case RenderPassType::SelectionOutline:
            if (callbacks.selection_outline)
                callbacks.selection_outline(context);
            break;
        case RenderPassType::Gizmos:
            if (callbacks.gizmos)
                callbacks.gizmos(context);
            break;
        // case RenderPassType::Grid:
        //     if (callbacks.grid)
        //         callbacks.grid(context);
        //     break;
        case RenderPassType::Shadows:
            if (callbacks.shadows)
                callbacks.shadows(context);
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
} // namespace Hybrid
