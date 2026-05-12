#pragma once

#include <functional>
#include <vector>

#include "runtime/modules/render/runtime/pipeline/render_context.h"
#include "runtime/modules/render/runtime/pipeline/render_graph.h"

namespace Hybrid
{
    struct RenderPipelineCallbacks
    {
        std::function<void(RenderContext&)> scene;
        std::function<void(RenderContext&)> skybox;
        std::function<void(RenderContext&)> picking;
        std::function<void(RenderContext&)> selection_mask;
        std::function<void(RenderContext&)> selection_overlay;
        std::function<void(RenderContext&)> world_gizmo;
        std::function<void(RenderContext&)> overlay_gizmo;
        std::function<void(RenderContext&)> grid;
        std::function<void(RenderContext&)> shadow;
        std::function<void(RenderContext&)> post_process;
        //std::function<void(RenderContext&)> debug_normals;
    };

    // Stage-1 pipeline extraction: owns pass order and flag-based dispatch only.
    class RenderPipeline
    {
    public:
        RenderPipeline();

        void execute(RenderContext& context, const RenderPipelineCallbacks& callbacks) const;
        const RenderGraphCompileResult& getCompiledGraph() const { return m_compiled_graph; }
        const std::vector<CompiledRenderGraphPass>& getPassGraph() const { return m_compiled_graph.passes; }
        const std::vector<RenderGraphResourceDesc>& getGraphResources() const { return m_compiled_graph.resources; }
        const RenderGraphValidationResult& validateGraph() const { return m_compiled_graph.validation; }
        std::string describeGraph() const;

    private:
        bool shouldRun(RenderPassType pass, RenderFlags flags) const;
        void invoke(RenderPassType pass, RenderContext& context, const RenderPipelineCallbacks& callbacks) const;

    private:
        RenderGraphCompileResult m_compiled_graph;
    };
} // namespace Hybrid
