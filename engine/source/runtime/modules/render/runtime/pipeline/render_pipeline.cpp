#include "render_pipeline.h"

#include <algorithm>

namespace Hybrid
{
    namespace
    {
        bool MatchesFeatureFlags(RenderFlags active, RenderFlags required)
        {
            return required == RenderFlags::None || HasFlag(active, required);
        }

        std::size_t FindPass(const std::vector<RenderGraphPassDesc>& passes,
                             RenderPassType type,
                             std::size_t fallback)
        {
            for (std::size_t index = 0; index < passes.size(); ++index)
            {
                if (passes[index].type == type)
                    return index;
            }
            return fallback;
        }
    } // namespace

    RenderPipeline::RenderPipeline()
        : m_base_graph(CreateDefaultRenderGraphBuild())
    {
        (void)rebuildGraph();
    }

    bool RenderPipeline::registerFeature(std::shared_ptr<IRenderFeature> feature)
    {
        if (!feature || feature->describe().name.empty())
            return false;
        const std::string name = feature->describe().name;
        const auto duplicate = std::find_if(m_features.begin(), m_features.end(), [&name](const auto& existing)
        {
            return existing && existing->describe().name == name;
        });
        if (duplicate != m_features.end())
            return false;

        m_features.push_back(std::move(feature));
        if (rebuildGraph())
            return true;
        m_features.pop_back();
        (void)rebuildGraph();
        return false;
    }

    bool RenderPipeline::unregisterFeature(const std::string& name)
    {
        const auto feature = std::find_if(m_features.begin(), m_features.end(), [&name](const auto& existing)
        {
            return existing && existing->describe().name == name;
        });
        if (feature == m_features.end())
            return false;
        m_features.erase(feature);
        return rebuildGraph();
    }

    bool RenderPipeline::rebuildGraph()
    {
        RenderGraphBuilder builder;
        for (const RenderGraphResourceDesc& resource : m_base_graph.resources)
            builder.addResource(resource);

        RenderGraphBlackboard blackboard;
        for (const RenderGraphResourceDesc& resource : m_base_graph.resources)
        {
            if (!blackboard.publish(resource.name, resource.name))
                return false;
        }
        for (const std::shared_ptr<IRenderFeature>& feature : m_features)
        {
            if (!feature || !feature->declareResources(builder, blackboard))
                return false;
        }

        RenderGraphBuildResult build = builder.build();
        build.passes = m_base_graph.passes;
        for (const std::shared_ptr<IRenderFeature>& feature : m_features)
        {
            const RenderFeatureDesc desc = feature->describe();
            RenderGraphPassDesc pass{};
            pass.name = desc.name;
            pass.type = RenderPassType::Custom;
            pass.required_flags = desc.required_flags;
            pass.editor_only = desc.editor_only;
            RenderGraphPassBuilder pass_builder(pass);
            feature->declarePass(pass_builder, blackboard);

            const std::size_t index = insertionIndex(build.passes, desc.injection_point);
            build.passes.insert(build.passes.begin() + static_cast<std::ptrdiff_t>(index), std::move(pass));
        }

        RenderGraphCompileResult compiled = CompileRenderGraph(build);
        if (!compiled.isValid())
            return false;
        m_compiled_graph = std::move(compiled);
        return true;
    }

    std::size_t RenderPipeline::insertionIndex(const std::vector<RenderGraphPassDesc>& passes,
                                                RenderFeatureInjectionPoint point)
    {
        switch (point)
        {
        case RenderFeatureInjectionPoint::BeforeScene:
            return FindPass(passes, RenderPassType::Scene, 0);
        case RenderFeatureInjectionPoint::AfterScene:
            return std::min(FindPass(passes, RenderPassType::Scene, passes.size()) + 1, passes.size());
        case RenderFeatureInjectionPoint::AfterLighting:
            return std::min(FindPass(passes, RenderPassType::Skybox, passes.size()) + 1, passes.size());
        case RenderFeatureInjectionPoint::BeforePostProcess:
            return FindPass(passes, RenderPassType::PostProcess, passes.size());
        case RenderFeatureInjectionPoint::AfterPostProcess:
            return std::min(FindPass(passes, RenderPassType::PostProcess, passes.size()) + 1, passes.size());
        case RenderFeatureInjectionPoint::BeforeEditorOverlay:
            return FindPass(passes, RenderPassType::Grid, passes.size());
        case RenderFeatureInjectionPoint::AfterEditorOverlay:
            return std::min(FindPass(passes, RenderPassType::OverlayGizmo, passes.size()) + 1, passes.size());
        }
        return passes.size();
    }

    void RenderPipeline::execute(RenderContext& context, const RenderPipelineCallbacks& callbacks) const
    {
        for (const CompiledRenderGraphPass& compiled_pass : m_compiled_graph.passes)
        {
            const RenderGraphPassDesc& pass_desc = compiled_pass.desc;
            if (!shouldRun(pass_desc, context.flags))
                continue;

            if (pass_desc.type == RenderPassType::Custom)
            {
                const auto feature = std::find_if(m_features.begin(), m_features.end(), [&pass_desc](const auto& item)
                {
                    return item && item->describe().name == pass_desc.name;
                });
                if (feature != m_features.end())
                    (*feature)->execute(context);
                continue;
            }
            invoke(pass_desc.type, context, callbacks);
        }
    }

    bool RenderPipeline::shouldRun(const RenderGraphPassDesc& pass, RenderFlags flags) const
    {
        if (pass.type == RenderPassType::Custom)
            return MatchesFeatureFlags(flags, pass.required_flags);

        switch (pass.type)
        {
        case RenderPassType::Scene:
        case RenderPassType::Skybox:
            return HasFlag(flags, RenderFlags::Scene) || HasFlag(flags, RenderFlags::PickingID) ||
                   HasFlag(flags, RenderFlags::SelectionHighlight);
        case RenderPassType::Picking: return HasFlag(flags, RenderFlags::PickingID);
        case RenderPassType::SelectionMask:
        case RenderPassType::SelectionOverlay: return HasFlag(flags, RenderFlags::SelectionHighlight);
        case RenderPassType::WorldGizmo:
        case RenderPassType::OverlayGizmo: return HasFlag(flags, RenderFlags::Gizmo);
        case RenderPassType::Grid: return HasFlag(flags, RenderFlags::Grid);
        case RenderPassType::Shadow: return HasFlag(flags, RenderFlags::Shadow);
        case RenderPassType::PostProcess: return HasFlag(flags, RenderFlags::PostProcess);
        case RenderPassType::Custom: return false;
        }
        return false;
    }

    void RenderPipeline::invoke(RenderPassType pass, RenderContext& context, const RenderPipelineCallbacks& callbacks) const
    {
        switch (pass)
        {
        case RenderPassType::Scene: if (callbacks.scene) callbacks.scene(context); break;
        case RenderPassType::Skybox: if (callbacks.skybox) callbacks.skybox(context); break;
        case RenderPassType::Picking: if (callbacks.picking) callbacks.picking(context); break;
        case RenderPassType::SelectionMask: if (callbacks.selection_mask) callbacks.selection_mask(context); break;
        case RenderPassType::SelectionOverlay: if (callbacks.selection_overlay) callbacks.selection_overlay(context); break;
        case RenderPassType::WorldGizmo: if (callbacks.world_gizmo) callbacks.world_gizmo(context); break;
        case RenderPassType::OverlayGizmo: if (callbacks.overlay_gizmo) callbacks.overlay_gizmo(context); break;
        case RenderPassType::Grid: if (callbacks.grid) callbacks.grid(context); break;
        case RenderPassType::Shadow: if (callbacks.shadow) callbacks.shadow(context); break;
        case RenderPassType::PostProcess: if (callbacks.post_process) callbacks.post_process(context); break;
        case RenderPassType::Custom: break;
        }
    }

    std::string RenderPipeline::describeGraph() const
    {
        std::vector<RenderGraphPassDesc> pass_descs;
        pass_descs.reserve(m_compiled_graph.passes.size());
        for (const CompiledRenderGraphPass& compiled_pass : m_compiled_graph.passes)
            pass_descs.push_back(compiled_pass.desc);
        return DescribeRenderGraph(pass_descs, m_compiled_graph.resources);
    }
} // namespace Hybrid
