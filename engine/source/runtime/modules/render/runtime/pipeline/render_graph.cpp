#include "render_graph.h"

#include <sstream>
#include <unordered_map>

namespace Hybrid
{
    namespace
    {
        bool IsReadAccess(RenderResourceAccess access)
        {
            return access == RenderResourceAccess::Read || access == RenderResourceAccess::ReadWrite;
        }

        bool IsWriteAccess(RenderResourceAccess access)
        {
            return access == RenderResourceAccess::Write || access == RenderResourceAccess::ReadWrite;
        }

        const char* ToString(RenderResourceId resource)
        {
            switch (resource)
            {
            case RenderResourceId::SceneColor: return "SceneColor";
            case RenderResourceId::SceneEntityID: return "SceneEntityID";
            case RenderResourceId::SceneDepth: return "SceneDepth";
            case RenderResourceId::SelectionMask: return "SelectionMask";
            case RenderResourceId::SelectionDepth: return "SelectionDepth";
            case RenderResourceId::ShadowDepth: return "ShadowDepth";
            default: return "Unknown";
            }
        }

        const char* ToString(RenderResourceAccess access)
        {
            switch (access)
            {
            case RenderResourceAccess::Read: return "read";
            case RenderResourceAccess::Write: return "write";
            case RenderResourceAccess::ReadWrite: return "read/write";
            default: return "unknown";
            }
        }

        const char* ToString(RenderFlags flags)
        {
            if (flags == RenderFlags::Shadow)
                return "Shadow";
            if (flags == RenderFlags::Scene)
                return "Scene";
            if (flags == RenderFlags::PickingID)
                return "PickingID";
            if (flags == RenderFlags::SelectionHighlight)
                return "SelectionHighlight";
            if (flags == RenderFlags::Grid)
                return "Grid";
            if (flags == RenderFlags::Gizmo)
                return "Gizmo";
            if (flags == RenderFlags::PostProcess)
                return "PostProcess";
            if (flags == (RenderFlags::Scene | RenderFlags::PickingID | RenderFlags::SelectionHighlight))
                return "Scene|PickingID|SelectionHighlight";
            return "Mixed";
        }

        const char* ToString(RenderGraphResourceKind kind)
        {
            switch (kind)
            {
            case RenderGraphResourceKind::Texture2D: return "Texture2D";
            case RenderGraphResourceKind::DepthTexture: return "DepthTexture";
            default: return "Unknown";
            }
        }

        const char* ToString(RenderGraphResourceFormat format)
        {
            switch (format)
            {
            case RenderGraphResourceFormat::RGBA8: return "RGBA8";
            case RenderGraphResourceFormat::R32UI: return "R32UI";
            case RenderGraphResourceFormat::R8: return "R8";
            case RenderGraphResourceFormat::Depth32F: return "Depth32F";
            default: return "Unknown";
            }
        }

        const char* ToString(RenderGraphResourceLifetime lifetime)
        {
            switch (lifetime)
            {
            case RenderGraphResourceLifetime::External: return "external";
            case RenderGraphResourceLifetime::Transient: return "transient";
            default: return "unknown";
            }
        }

        std::string ResourceName(RenderResourceId resource)
        {
            return ToString(resource);
        }

        std::string ResourceName(const RenderResourceUsage& usage)
        {
            return usage.resource_name.empty() ? ResourceName(usage.resource) : usage.resource_name;
        }

        const RenderGraphResourceDesc* FindResourceDesc(const std::vector<RenderGraphResourceDesc>& resources,
                                                        const std::string& name)
        {
            for (const RenderGraphResourceDesc& resource : resources)
            {
                if (resource.name == name)
                    return &resource;
            }
            return nullptr;
        }

        bool IsDepthResource(const RenderGraphResourceDesc& resource)
        {
            return resource.kind == RenderGraphResourceKind::DepthTexture ||
                   resource.format == RenderGraphResourceFormat::Depth32F;
        }

        void AddIssue(RenderGraphValidationResult& result,
                      RenderGraphIssueSeverity severity,
                      std::size_t pass_index,
                      const std::string& pass_name,
                      const std::string& message)
        {
            result.issues.push_back({ severity, pass_index, pass_name, message });
        }
    } // namespace

    RenderGraphPassBuilder::RenderGraphPassBuilder(RenderGraphPassDesc& pass)
        : m_pass(pass)
    {
    }

    RenderGraphPassBuilder& RenderGraphPassBuilder::read(RenderResourceId resource)
    {
        m_pass.resources.push_back({ resource, RenderResourceAccess::Read, ResourceName(resource) });
        return *this;
    }

    RenderGraphPassBuilder& RenderGraphPassBuilder::write(RenderResourceId resource)
    {
        m_pass.resources.push_back({ resource, RenderResourceAccess::Write, ResourceName(resource) });
        return *this;
    }

    RenderGraphPassBuilder& RenderGraphPassBuilder::readWrite(RenderResourceId resource)
    {
        m_pass.resources.push_back({ resource, RenderResourceAccess::ReadWrite, ResourceName(resource) });
        return *this;
    }

    RenderGraphPassBuilder& RenderGraphPassBuilder::read(const std::string& resource_name)
    {
        m_pass.resources.push_back({ RenderResourceId::Count, RenderResourceAccess::Read, resource_name });
        return *this;
    }

    RenderGraphPassBuilder& RenderGraphPassBuilder::write(const std::string& resource_name)
    {
        m_pass.resources.push_back({ RenderResourceId::Count, RenderResourceAccess::Write, resource_name });
        return *this;
    }

    RenderGraphPassBuilder& RenderGraphPassBuilder::readWrite(const std::string& resource_name)
    {
        m_pass.resources.push_back({ RenderResourceId::Count, RenderResourceAccess::ReadWrite, resource_name });
        return *this;
    }

    RenderGraphBuilder& RenderGraphBuilder::addResource(const RenderGraphResourceDesc& resource)
    {
        m_resources.push_back(resource);
        return *this;
    }

    RenderGraphBuilder& RenderGraphBuilder::addTextureResource(const std::string& name,
                                                                 RenderGraphResourceFormat format,
                                                                 RenderGraphResourceLifetime lifetime,
                                                                 RenderGraphResourceKind kind)
    {
        m_resources.push_back({ name, RenderResourceId::Count, kind, format, lifetime });
        return *this;
    }

    RenderGraphPassBuilder RenderGraphBuilder::addPass(const char* name,
                                                       RenderPassType type,
                                                       RenderFlags required_flags,
                                                       bool editor_only)
    {
        m_passes.push_back({ name, type, required_flags, editor_only, {} });
        return RenderGraphPassBuilder(m_passes.back());
    }

    RenderGraphBuildResult RenderGraphBuilder::build() const
    {
        return { m_resources, m_passes };
    }

    RenderGraphBuildResult CreateDefaultRenderGraphBuild()
    {
        RenderGraphBuilder builder;
        builder.addResource({ "SceneColor", RenderResourceId::SceneColor, RenderGraphResourceKind::Texture2D, RenderGraphResourceFormat::RGBA8, RenderGraphResourceLifetime::External });
        builder.addResource({ "SceneEntityID", RenderResourceId::SceneEntityID, RenderGraphResourceKind::Texture2D, RenderGraphResourceFormat::R32UI, RenderGraphResourceLifetime::External });
        builder.addResource({ "SceneDepth", RenderResourceId::SceneDepth, RenderGraphResourceKind::DepthTexture, RenderGraphResourceFormat::Depth32F, RenderGraphResourceLifetime::External });
        builder.addResource({ "SelectionMask", RenderResourceId::SelectionMask, RenderGraphResourceKind::Texture2D, RenderGraphResourceFormat::R8, RenderGraphResourceLifetime::External });
        builder.addResource({ "SelectionDepth", RenderResourceId::SelectionDepth, RenderGraphResourceKind::DepthTexture, RenderGraphResourceFormat::Depth32F, RenderGraphResourceLifetime::External });
        builder.addResource({ "ShadowDepth", RenderResourceId::ShadowDepth, RenderGraphResourceKind::DepthTexture, RenderGraphResourceFormat::Depth32F, RenderGraphResourceLifetime::External });
        builder.addResource({ "ShadowDepth1", RenderResourceId::Count, RenderGraphResourceKind::DepthTexture, RenderGraphResourceFormat::Depth32F, RenderGraphResourceLifetime::External });
        builder.addResource({ "ShadowDepth2", RenderResourceId::Count, RenderGraphResourceKind::DepthTexture, RenderGraphResourceFormat::Depth32F, RenderGraphResourceLifetime::External });
        builder.addResource({ "ShadowDepth3", RenderResourceId::Count, RenderGraphResourceKind::DepthTexture, RenderGraphResourceFormat::Depth32F, RenderGraphResourceLifetime::External });
        builder.addTextureResource("PostProcessInput", RenderGraphResourceFormat::RGBA8,
                                   RenderGraphResourceLifetime::Transient);
        builder.addTextureResource("SelectionOverlayInput", RenderGraphResourceFormat::RGBA8,
                                   RenderGraphResourceLifetime::Transient);

        builder.addPass("Shadow", RenderPassType::Shadow, RenderFlags::Shadow)
            .write(RenderResourceId::ShadowDepth)
            .write("ShadowDepth1")
            .write("ShadowDepth2")
            .write("ShadowDepth3");

        builder.addPass("Scene", RenderPassType::Scene, RenderFlags::Scene | RenderFlags::PickingID | RenderFlags::SelectionHighlight)
            .read(RenderResourceId::ShadowDepth)
            .read("ShadowDepth1")
            .read("ShadowDepth2")
            .read("ShadowDepth3")
            .write(RenderResourceId::SceneColor)
            .write(RenderResourceId::SceneEntityID)
            .write(RenderResourceId::SceneDepth);

        builder.addPass("Skybox", RenderPassType::Skybox, RenderFlags::Scene | RenderFlags::PickingID | RenderFlags::SelectionHighlight)
            .read(RenderResourceId::SceneDepth)
            .readWrite(RenderResourceId::SceneColor);

        builder.addPass("Picking", RenderPassType::Picking, RenderFlags::PickingID, true)
            .read(RenderResourceId::SceneEntityID);

        builder.addPass("SelectionMask", RenderPassType::SelectionMask, RenderFlags::SelectionHighlight, true)
            .read(RenderResourceId::SceneDepth)
            .write(RenderResourceId::SelectionMask)
            .write(RenderResourceId::SelectionDepth);

        builder.addPass("SelectionOverlay", RenderPassType::SelectionOverlay, RenderFlags::SelectionHighlight, true)
            .read(RenderResourceId::SceneColor)
            .write("SelectionOverlayInput")
            .read(RenderResourceId::SceneDepth)
            .read(RenderResourceId::SelectionMask)
            .read(RenderResourceId::SelectionDepth)
            .write(RenderResourceId::SceneColor);

        builder.addPass("Grid", RenderPassType::Grid, RenderFlags::Grid, true)
            .readWrite(RenderResourceId::SceneColor);

        builder.addPass("WorldGizmo", RenderPassType::WorldGizmo, RenderFlags::Gizmo, true)
            .read(RenderResourceId::SceneDepth)
            .readWrite(RenderResourceId::SceneColor);

        builder.addPass("OverlayGizmo", RenderPassType::OverlayGizmo, RenderFlags::Gizmo, true)
            .readWrite(RenderResourceId::SceneColor);

        builder.addPass("PostProcess", RenderPassType::PostProcess, RenderFlags::PostProcess)
            .write("PostProcessInput")
            .readWrite(RenderResourceId::SceneColor);

        return builder.build();
    }

    bool RenderGraphCompileResult::isValid() const
    {
        return !validation.hasErrors();
    }

    RenderGraphCompileResult CompileRenderGraph(const RenderGraphBuildResult& build_result)
    {
        RenderGraphCompileResult compiled;
        compiled.resources = build_result.resources;
        compiled.validation = ValidateRenderGraph(build_result.passes, build_result.resources);
        compiled.passes.reserve(build_result.passes.size());

        for (std::size_t pass_index = 0; pass_index < build_result.passes.size(); ++pass_index)
        {
            compiled.passes.push_back({ pass_index, build_result.passes[pass_index] });
        }

        return compiled;
    }

    std::vector<RenderGraphResourceDesc> CreateDefaultRenderGraphResources()
    {
        return CreateDefaultRenderGraphBuild().resources;
    }

    std::vector<RenderGraphPassDesc> CreateDefaultRenderGraph()
    {
        return CreateDefaultRenderGraphBuild().passes;
    }

    bool RenderGraphValidationResult::hasErrors() const
    {
        for (const RenderGraphIssue& issue : issues)
        {
            if (issue.severity == RenderGraphIssueSeverity::Error)
                return true;
        }
        return false;
    }

    RenderGraphValidationResult ValidateRenderGraph(const std::vector<RenderGraphPassDesc>& graph,
                                                    const std::vector<RenderGraphResourceDesc>& resources)
    {
        RenderGraphValidationResult result;
        struct ResourceState
        {
            bool has_write = false;
            bool last_access_was_write_only = false;
            std::string last_writer;
        };
        std::unordered_map<std::string, ResourceState> resource_states;

        for (const RenderGraphResourceDesc& resource : resources)
        {
            if (resource.name.empty())
            {
                RenderGraphIssue issue;
                issue.severity = RenderGraphIssueSeverity::Error;
                issue.message = "Render resource has no name.";
                result.issues.push_back(issue);
                continue;
            }
            if (resource_states.find(resource.name) != resource_states.end())
            {
                RenderGraphIssue issue;
                issue.severity = RenderGraphIssueSeverity::Error;
                issue.message = std::string("Render resource is registered more than once: ") + resource.name;
                result.issues.push_back(issue);
            }
            resource_states.emplace(resource.name, ResourceState{});
        }

        for (std::size_t pass_index = 0; pass_index < graph.size(); ++pass_index)
        {
            const RenderGraphPassDesc& pass = graph[pass_index];
            const std::string& pass_name = pass.name;
            if (pass_name.empty())
            {
                AddIssue(result, RenderGraphIssueSeverity::Warning, pass_index, pass_name, "Render pass has no debug name.");
            }

            for (const RenderResourceUsage& usage : pass.resources)
            {
                const std::string resource_name = ResourceName(usage);
                const RenderGraphResourceDesc* resource_desc = FindResourceDesc(resources, resource_name);
                if (resource_desc == nullptr)
                {
                    std::ostringstream message;
                    message << "Uses unregistered resource " << resource_name << '.';
                    AddIssue(result, RenderGraphIssueSeverity::Error, pass_index, pass_name, message.str());
                    continue;
                }
                ResourceState& state = resource_states[resource_name];

                if (resource_desc->lifetime == RenderGraphResourceLifetime::Transient &&
                    IsReadAccess(usage.access) &&
                    !state.has_write)
                {
                    std::ostringstream message;
                    message << "Reads transient resource " << resource_name << " before it has a producer.";
                    AddIssue(result, RenderGraphIssueSeverity::Error, pass_index, pass_name, message.str());
                }

                if (usage.resource != RenderResourceId::Count && IsDepthResource(*resource_desc) && usage.resource != RenderResourceId::SceneDepth &&
                    usage.resource != RenderResourceId::SelectionDepth && usage.resource != RenderResourceId::ShadowDepth)
                {
                    std::ostringstream message;
                    message << "Depth resource metadata does not match resource id " << ToString(usage.resource) << '.';
                    AddIssue(result, RenderGraphIssueSeverity::Warning, pass_index, pass_name, message.str());
                }

                if (IsReadAccess(usage.access) && !state.has_write)
                {
                    std::ostringstream message;
                    message << "Reads " << resource_name << " before any previous pass writes it.";
                    AddIssue(result, RenderGraphIssueSeverity::Error, pass_index, pass_name, message.str());
                }

                if (usage.access == RenderResourceAccess::Write && state.last_access_was_write_only)
                {
                    std::ostringstream message;
                    message << "Writes " << resource_name << " after pass '" << state.last_writer
                            << "' also wrote it without an explicit read/write dependency.";
                    AddIssue(result, RenderGraphIssueSeverity::Error, pass_index, pass_name, message.str());
                }

                if (IsWriteAccess(usage.access))
                {
                    state.has_write = true;
                    state.last_writer = pass_name;
                    state.last_access_was_write_only = usage.access == RenderResourceAccess::Write;
                }
                else
                {
                    state.last_access_was_write_only = false;
                }
            }
        }

        return result;
    }

    RenderGraphValidationResult ValidateRenderGraph(const std::vector<RenderGraphPassDesc>& graph)
    {
        return ValidateRenderGraph(graph, CreateDefaultRenderGraphResources());
    }

    std::string DescribeRenderGraph(const std::vector<RenderGraphPassDesc>& graph,
                                    const std::vector<RenderGraphResourceDesc>& resources)
    {
        std::ostringstream stream;
        stream << "RenderGraph resources: " << resources.size() << '\n';
        for (const RenderGraphResourceDesc& resource : resources)
        {
            stream << "- " << resource.name
                   << " id=" << ToString(resource.id)
                   << " kind=" << ToString(resource.kind)
                   << " format=" << ToString(resource.format)
                   << " lifetime=" << ToString(resource.lifetime)
                   << '\n';
        }

        stream << "RenderGraph passes: " << graph.size() << '\n';

        for (std::size_t pass_index = 0; pass_index < graph.size(); ++pass_index)
        {
            const RenderGraphPassDesc& pass = graph[pass_index];
            stream << pass_index << ": " << pass.name
                   << " flags=" << ToString(pass.required_flags)
                   << " editor_only=" << (pass.editor_only ? "true" : "false");

            if (!pass.resources.empty())
            {
                stream << " resources=[";
                for (std::size_t resource_index = 0; resource_index < pass.resources.size(); ++resource_index)
                {
                    const RenderResourceUsage& usage = pass.resources[resource_index];
                    if (resource_index != 0)
                        stream << ", ";
                    stream << ResourceName(usage) << ':' << ToString(usage.access);
                }
                stream << ']';
            }

            stream << '\n';
        }

        return stream.str();
    }

    std::string DescribeRenderGraph(const std::vector<RenderGraphPassDesc>& graph)
    {
        return DescribeRenderGraph(graph, CreateDefaultRenderGraphResources());
    }
} // namespace Hybrid
