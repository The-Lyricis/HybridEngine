#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "runtime/modules/render/runtime/pipeline/render_graph.h"

namespace Hybrid
{
    struct RenderContext;

    // These are stable semantic seams, not backend command-encoder escape hatches.
    // A feature declares graph dependencies and receives a RenderContext only when
    // the graph reaches its declared pass.
    enum class RenderFeatureInjectionPoint : unsigned char
    {
        BeforeScene,
        AfterScene,
        AfterLighting,
        BeforePostProcess,
        AfterPostProcess,
        BeforeEditorOverlay,
        AfterEditorOverlay,
    };

    class RenderGraphBlackboard
    {
    public:
        bool publish(const std::string& key, const std::string& resource_name);
        std::optional<std::string> find(const std::string& key) const;
        void clear();

    private:
        std::vector<std::pair<std::string, std::string>> m_entries;
    };

    struct RenderFeatureDesc
    {
        std::string name;
        RenderFeatureInjectionPoint injection_point = RenderFeatureInjectionPoint::BeforePostProcess;
        RenderFlags required_flags = RenderFlags::None;
        bool editor_only = false;
    };

    // Extension modules own their shader/pipeline state, but graph-owned textures
    // are described here and never exposed as OpenGL/Metal/Vulkan native objects.
    class IRenderFeature
    {
    public:
        virtual ~IRenderFeature() = default;
        virtual RenderFeatureDesc describe() const = 0;
        virtual bool declareResources(RenderGraphBuilder& builder,
                                      RenderGraphBlackboard& blackboard) const
        {
            (void)builder;
            (void)blackboard;
            return true;
        }
        virtual void declarePass(RenderGraphPassBuilder& pass,
                                 const RenderGraphBlackboard& blackboard) const = 0;
        virtual void execute(RenderContext& context) = 0;
    };
} // namespace Hybrid
