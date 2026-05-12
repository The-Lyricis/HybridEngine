#pragma once

#include <string>
#include <vector>

#include "runtime/modules/render/runtime/pipeline/render_pass_type.h"
#include "runtime/modules/render/runtime/render_flags.h"

namespace Hybrid
{
    enum class RenderResourceId : unsigned char
    {
        SceneColor,
        SceneEntityID,
        SceneDepth,
        SelectionMask,
        SelectionDepth,
        ShadowDepth,
        Count,
    };

    enum class RenderResourceAccess : unsigned char
    {
        Read,
        Write,
        ReadWrite,
    };

    enum class RenderGraphResourceKind : unsigned char
    {
        Texture2D,
        DepthTexture,
    };

    enum class RenderGraphResourceFormat : unsigned char
    {
        RGBA8,
        R32UI,
        R8,
        Depth32F,
    };

    enum class RenderGraphResourceLifetime : unsigned char
    {
        External,
        Transient,
    };

    struct RenderGraphResourceDesc
    {
        const char* name = "";
        RenderResourceId id = RenderResourceId::SceneColor;
        RenderGraphResourceKind kind = RenderGraphResourceKind::Texture2D;
        RenderGraphResourceFormat format = RenderGraphResourceFormat::RGBA8;
        RenderGraphResourceLifetime lifetime = RenderGraphResourceLifetime::External;
    };

    struct RenderResourceUsage
    {
        RenderResourceId resource;
        RenderResourceAccess access;
    };

    struct RenderGraphPassDesc
    {
        const char* name = "";
        RenderPassType type = RenderPassType::Scene;
        RenderFlags required_flags = RenderFlags::None;
        bool editor_only = false;
        std::vector<RenderResourceUsage> resources;
    };

    enum class RenderGraphIssueSeverity : unsigned char
    {
        Warning,
        Error,
    };

    struct RenderGraphIssue
    {
        RenderGraphIssueSeverity severity = RenderGraphIssueSeverity::Warning;
        std::size_t pass_index = 0;
        const char* pass_name = "";
        std::string message;
    };

    struct RenderGraphValidationResult
    {
        std::vector<RenderGraphIssue> issues;

        bool hasErrors() const;
    };

    struct RenderGraphBuildResult
    {
        std::vector<RenderGraphResourceDesc> resources;
        std::vector<RenderGraphPassDesc> passes;
    };

    struct CompiledRenderGraphPass
    {
        std::size_t execution_index = 0;
        RenderGraphPassDesc desc;
    };

    struct RenderGraphCompileResult
    {
        std::vector<RenderGraphResourceDesc> resources;
        std::vector<CompiledRenderGraphPass> passes;
        RenderGraphValidationResult validation;

        bool isValid() const;
    };

    class RenderGraphPassBuilder
    {
    public:
        explicit RenderGraphPassBuilder(RenderGraphPassDesc& pass);

        RenderGraphPassBuilder& read(RenderResourceId resource);
        RenderGraphPassBuilder& write(RenderResourceId resource);
        RenderGraphPassBuilder& readWrite(RenderResourceId resource);

    private:
        RenderGraphPassDesc& m_pass;
    };

    class RenderGraphBuilder
    {
    public:
        RenderGraphBuilder& addResource(const RenderGraphResourceDesc& resource);
        RenderGraphPassBuilder addPass(const char* name,
                                       RenderPassType type,
                                       RenderFlags required_flags,
                                       bool editor_only = false);

        RenderGraphBuildResult build() const;

    private:
        std::vector<RenderGraphResourceDesc> m_resources;
        std::vector<RenderGraphPassDesc> m_passes;
    };

    RenderGraphBuildResult CreateDefaultRenderGraphBuild();
    RenderGraphCompileResult CompileRenderGraph(const RenderGraphBuildResult& build_result);
    std::vector<RenderGraphResourceDesc> CreateDefaultRenderGraphResources();
    std::vector<RenderGraphPassDesc> CreateDefaultRenderGraph();
    RenderGraphValidationResult ValidateRenderGraph(const std::vector<RenderGraphPassDesc>& graph,
                                                    const std::vector<RenderGraphResourceDesc>& resources);
    RenderGraphValidationResult ValidateRenderGraph(const std::vector<RenderGraphPassDesc>& graph);
    std::string DescribeRenderGraph(const std::vector<RenderGraphPassDesc>& graph,
                                    const std::vector<RenderGraphResourceDesc>& resources);
    std::string DescribeRenderGraph(const std::vector<RenderGraphPassDesc>& graph);
} // namespace Hybrid
