#include "render_graph.h"

#include <array>
#include <sstream>

namespace Hybrid
{
    namespace
    {
        constexpr std::size_t RenderResourceCount = static_cast<std::size_t>(RenderResourceId::Count);

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

        const RenderGraphResourceDesc* FindResourceDesc(const std::vector<RenderGraphResourceDesc>& resources,
                                                        RenderResourceId id)
        {
            for (const RenderGraphResourceDesc& resource : resources)
            {
                if (resource.id == id)
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
                      const char* pass_name,
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
        m_pass.resources.push_back({ resource, RenderResourceAccess::Read });
        return *this;
    }

    RenderGraphPassBuilder& RenderGraphPassBuilder::write(RenderResourceId resource)
    {
        m_pass.resources.push_back({ resource, RenderResourceAccess::Write });
        return *this;
    }

    RenderGraphPassBuilder& RenderGraphPassBuilder::readWrite(RenderResourceId resource)
    {
        m_pass.resources.push_back({ resource, RenderResourceAccess::ReadWrite });
        return *this;
    }

    RenderGraphBuilder& RenderGraphBuilder::addResource(const RenderGraphResourceDesc& resource)
    {
        m_resources.push_back(resource);
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

        builder.addPass("Shadow", RenderPassType::Shadow, RenderFlags::Shadow)
            .write(RenderResourceId::ShadowDepth);

        builder.addPass("Scene", RenderPassType::Scene, RenderFlags::Scene | RenderFlags::PickingID | RenderFlags::SelectionHighlight)
            .read(RenderResourceId::ShadowDepth)
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
            .readWrite(RenderResourceId::SceneColor)
            .read(RenderResourceId::SceneDepth)
            .read(RenderResourceId::SelectionMask)
            .read(RenderResourceId::SelectionDepth);

        builder.addPass("Grid", RenderPassType::Grid, RenderFlags::Grid, true)
            .readWrite(RenderResourceId::SceneColor);

        builder.addPass("WorldGizmo", RenderPassType::WorldGizmo, RenderFlags::Gizmo, true)
            .read(RenderResourceId::SceneDepth)
            .readWrite(RenderResourceId::SceneColor);

        builder.addPass("OverlayGizmo", RenderPassType::OverlayGizmo, RenderFlags::Gizmo, true)
            .readWrite(RenderResourceId::SceneColor);

        builder.addPass("PostProcess", RenderPassType::PostProcess, RenderFlags::PostProcess)
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
        std::array<bool, RenderResourceCount> has_write {};
        std::array<bool, RenderResourceCount> last_access_was_write_only {};
        std::array<const char*, RenderResourceCount> last_writer {};
        std::array<bool, RenderResourceCount> registered_resources {};

        for (const RenderGraphResourceDesc& resource : resources)
        {
            const std::size_t resource_index = static_cast<std::size_t>(resource.id);
            if (resource_index >= RenderResourceCount)
                continue;

            if (registered_resources[resource_index])
            {
                RenderGraphIssue issue;
                issue.severity = RenderGraphIssueSeverity::Error;
                issue.message = std::string("Render resource is registered more than once: ") + ToString(resource.id);
                result.issues.push_back(issue);
            }

            registered_resources[resource_index] = true;
        }

        for (std::size_t pass_index = 0; pass_index < graph.size(); ++pass_index)
        {
            const RenderGraphPassDesc& pass = graph[pass_index];
            const char* pass_name = pass.name != nullptr ? pass.name : "";
            if (pass_name[0] == '\0')
            {
                AddIssue(result, RenderGraphIssueSeverity::Warning, pass_index, pass_name, "Render pass has no debug name.");
            }

            for (const RenderResourceUsage& usage : pass.resources)
            {
                const std::size_t resource_index = static_cast<std::size_t>(usage.resource);
                if (resource_index >= RenderResourceCount)
                {
                    AddIssue(result, RenderGraphIssueSeverity::Error, pass_index, pass_name, "Render pass references an unknown resource.");
                    continue;
                }

                const RenderGraphResourceDesc* resource_desc = FindResourceDesc(resources, usage.resource);
                if (resource_desc == nullptr)
                {
                    std::ostringstream message;
                    message << "Uses unregistered resource " << ToString(usage.resource) << '.';
                    AddIssue(result, RenderGraphIssueSeverity::Error, pass_index, pass_name, message.str());
                    continue;
                }

                if (resource_desc->lifetime == RenderGraphResourceLifetime::Transient &&
                    IsReadAccess(usage.access) &&
                    !has_write[resource_index])
                {
                    std::ostringstream message;
                    message << "Reads transient resource " << ToString(usage.resource) << " before it has a producer.";
                    AddIssue(result, RenderGraphIssueSeverity::Error, pass_index, pass_name, message.str());
                }

                if (IsDepthResource(*resource_desc) && usage.resource != RenderResourceId::SceneDepth &&
                    usage.resource != RenderResourceId::SelectionDepth && usage.resource != RenderResourceId::ShadowDepth)
                {
                    std::ostringstream message;
                    message << "Depth resource metadata does not match resource id " << ToString(usage.resource) << '.';
                    AddIssue(result, RenderGraphIssueSeverity::Warning, pass_index, pass_name, message.str());
                }

                if (IsReadAccess(usage.access) && !has_write[resource_index])
                {
                    std::ostringstream message;
                    message << "Reads " << ToString(usage.resource) << " before any previous pass writes it.";
                    AddIssue(result, RenderGraphIssueSeverity::Error, pass_index, pass_name, message.str());
                }

                if (usage.access == RenderResourceAccess::Write && last_access_was_write_only[resource_index])
                {
                    std::ostringstream message;
                    message << "Writes " << ToString(usage.resource) << " after pass '" << last_writer[resource_index]
                            << "' also wrote it without an explicit read/write dependency.";
                    AddIssue(result, RenderGraphIssueSeverity::Error, pass_index, pass_name, message.str());
                }

                if (IsWriteAccess(usage.access))
                {
                    has_write[resource_index] = true;
                    last_writer[resource_index] = pass_name;
                    last_access_was_write_only[resource_index] = usage.access == RenderResourceAccess::Write;
                }
                else
                {
                    last_access_was_write_only[resource_index] = false;
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
            stream << pass_index << ": " << (pass.name != nullptr ? pass.name : "")
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
                    stream << ToString(usage.resource) << ':' << ToString(usage.access);
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
