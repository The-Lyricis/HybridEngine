#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "runtime/modules/render/runtime/pipeline/render_context.h"
#include "runtime/modules/render/runtime/pipeline/render_feature.h"
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

    class IRenderPipeline
    {
    public:
        virtual ~IRenderPipeline() = default;
        virtual void execute(RenderContext& context, const RenderPipelineCallbacks& callbacks) const = 0;
        virtual bool registerFeature(std::shared_ptr<IRenderFeature> feature) = 0;
        virtual bool unregisterFeature(const std::string& name) = 0;
        virtual const RenderGraphCompileResult& getCompiledGraph() const = 0;
    };

    // The default engine pipeline. Custom projects may replace it through the
    // IRenderPipeline contract or extend it safely with IRenderFeature modules.
    class RenderPipeline final : public IRenderPipeline
    {
    public:
        RenderPipeline();

        void execute(RenderContext& context, const RenderPipelineCallbacks& callbacks) const override;
        bool registerFeature(std::shared_ptr<IRenderFeature> feature) override;
        bool unregisterFeature(const std::string& name) override;
        const RenderGraphCompileResult& getCompiledGraph() const override { return m_compiled_graph; }
        const std::vector<CompiledRenderGraphPass>& getPassGraph() const { return m_compiled_graph.passes; }
        const std::vector<RenderGraphResourceDesc>& getGraphResources() const { return m_compiled_graph.resources; }
        const RenderGraphValidationResult& validateGraph() const { return m_compiled_graph.validation; }
        std::string describeGraph() const;

    private:
        bool shouldRun(const RenderGraphPassDesc& pass, RenderFlags flags) const;
        void invoke(RenderPassType pass, RenderContext& context, const RenderPipelineCallbacks& callbacks) const;
        bool rebuildGraph();
        static std::size_t insertionIndex(const std::vector<RenderGraphPassDesc>& passes,
                                          RenderFeatureInjectionPoint point);

    private:
        RenderGraphBuildResult m_base_graph;
        RenderGraphCompileResult m_compiled_graph;
        std::vector<std::shared_ptr<IRenderFeature>> m_features;
    };
} // namespace Hybrid
