#include "render_graph_resources.h"

#include <algorithm>
#include <unordered_set>

namespace Hybrid
{
    namespace
    {
        RhiFormat ToRhiFormat(RenderGraphResourceFormat format)
        {
            switch (format)
            {
            case RenderGraphResourceFormat::RGBA8: return RhiFormat::RGBA8Unorm;
            case RenderGraphResourceFormat::R32UI: return RhiFormat::R32Uint;
            case RenderGraphResourceFormat::R8: return RhiFormat::R8Unorm;
            case RenderGraphResourceFormat::Depth32F: return RhiFormat::Depth32Float;
            }
            return RhiFormat::Unknown;
        }

        bool SameTextureDesc(const RhiTextureDesc& lhs, const RhiTextureDesc& rhs)
        {
            return lhs.type == rhs.type && lhs.format == rhs.format && lhs.width == rhs.width &&
                   lhs.height == rhs.height && lhs.layers == rhs.layers && lhs.mip_levels == rhs.mip_levels &&
                   lhs.render_target == rhs.render_target && lhs.shader_read == rhs.shader_read;
        }
    } // namespace

    RenderGraphResourceRegistry::~RenderGraphResourceRegistry()
    {
        shutdown();
    }

    void RenderGraphResourceRegistry::clearExternalResources()
    {
        m_external_resources.clear();
    }

    void RenderGraphResourceRegistry::importExternalResource(const std::string& name, TextureViewHandle view)
    {
        if (!name.empty() && view)
            m_external_resources[name] = view;
    }

    RhiResult<RhiTextureDesc> RenderGraphResourceRegistry::makeTextureDesc(
        const RenderGraphResourceDesc& resource, uint32_t width, uint32_t height)
    {
        const RhiFormat format = ToRhiFormat(resource.format);
        if (format == RhiFormat::Unknown)
            return RhiResult<RhiTextureDesc>::Failure(RhiErrorCode::InvalidArgument,
                                                       "graph resource has an unsupported texture format");

        RhiTextureDesc desc{};
        desc.format = format;
        desc.width = std::max(1u, width);
        desc.height = std::max(1u, height);
        desc.render_target = true;
        desc.shader_read = true;
        desc.debug_name = "RenderGraph." + resource.name;
        return RhiResult<RhiTextureDesc>::Success(std::move(desc));
    }

    void RenderGraphResourceRegistry::destroyOwnedTexture(OwnedTexture& texture)
    {
        if (!m_device)
            return;
        if (texture.view)
            (void)m_device->destroyTextureView(texture.view);
        if (texture.texture)
            (void)m_device->destroyTexture(texture.texture);
        texture = {};
    }

    RhiStatus RenderGraphResourceRegistry::prepare(IRenderDevice& device,
                                                    const RenderGraphCompileResult& graph,
                                                    uint32_t width,
                                                    uint32_t height)
    {
        if (!graph.isValid())
            return RhiStatus::Failure(RhiErrorCode::InvalidState, "cannot materialize an invalid render graph");
        if (m_device && m_device != &device)
            shutdown();
        m_device = &device;

        std::unordered_set<std::string> active_transients;
        for (const RenderGraphResourceDesc& resource : graph.resources)
        {
            if (resource.lifetime != RenderGraphResourceLifetime::Transient)
                continue;
            if (resource.name.empty())
                return RhiStatus::Failure(RhiErrorCode::InvalidArgument, "transient graph resource has no name");
            active_transients.insert(resource.name);

            const auto desc = makeTextureDesc(resource, width, height);
            if (!desc)
                return {desc.error};
            auto existing = m_owned_resources.find(resource.name);
            if (existing != m_owned_resources.end() && SameTextureDesc(existing->second.desc, desc.value))
                continue;
            if (existing != m_owned_resources.end())
            {
                destroyOwnedTexture(existing->second);
                m_owned_resources.erase(existing);
            }

            const auto texture = device.createTexture(desc.value);
            if (!texture)
                return {texture.error};
            const auto view = device.createTextureView(texture.value);
            if (!view)
            {
                (void)device.destroyTexture(texture.value);
                return {view.error};
            }
            m_owned_resources.emplace(resource.name, OwnedTexture{texture.value, view.value, desc.value});
        }

        for (auto it = m_owned_resources.begin(); it != m_owned_resources.end();)
        {
            if (active_transients.find(it->first) != active_transients.end())
            {
                ++it;
                continue;
            }
            destroyOwnedTexture(it->second);
            it = m_owned_resources.erase(it);
        }
        return RhiStatus::Success();
    }

    RhiResult<TextureViewHandle> RenderGraphResourceRegistry::texture(const std::string& name) const
    {
        if (const auto external = m_external_resources.find(name); external != m_external_resources.end())
            return RhiResult<TextureViewHandle>::Success(external->second);
        if (const auto owned = m_owned_resources.find(name); owned != m_owned_resources.end())
            return RhiResult<TextureViewHandle>::Success(owned->second.view);
        return RhiResult<TextureViewHandle>::Failure(RhiErrorCode::InvalidHandle,
                                                      "render graph resource is not materialized: " + name);
    }

    void RenderGraphResourceRegistry::shutdown()
    {
        for (auto& entry : m_owned_resources)
            destroyOwnedTexture(entry.second);
        m_owned_resources.clear();
        m_external_resources.clear();
        m_device = nullptr;
    }
} // namespace Hybrid
