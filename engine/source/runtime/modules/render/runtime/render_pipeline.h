#pragma once

#include <functional>
#include <vector>

#include "runtime/modules/render/runtime/render_context.h"
#include "runtime/modules/render/runtime/render_flags.h"

namespace Hybrid
{
    enum class RenderPassType : unsigned char
    {
        Forward,
        Picking,
        SelectionOutline,
        Gizmos,
        Grid,
        Shadows,
        PostProcess,
        DebugNormals
    };

    struct RenderPipelineCallbacks
    {
        std::function<void(RenderContext&)> forward;
        std::function<void(RenderContext&)> picking;
        std::function<void(RenderContext&)> selection_outline;
        std::function<void(RenderContext&)> gizmos;
        std::function<void(RenderContext&)> grid;
        std::function<void(RenderContext&)> shadows;
        std::function<void(RenderContext&)> post_process;
        std::function<void(RenderContext&)> debug_normals;
    };

    // Stage-1 pipeline extraction: owns pass order and flag-based dispatch only.
    class RenderPipeline
    {
    public:
        RenderPipeline();

        void execute(RenderContext& context, const RenderPipelineCallbacks& callbacks) const;

    private:
        bool shouldRun(RenderPassType pass, RenderFlags flags) const;
        void invoke(RenderPassType pass, RenderContext& context, const RenderPipelineCallbacks& callbacks) const;

    private:
        std::vector<RenderPassType> m_pass_order;
    };
} // namespace Hybrid
