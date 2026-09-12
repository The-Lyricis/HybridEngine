#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

#include "runtime/modules/render/rhi/render_device.h"
#include "runtime/modules/render/runtime/pipeline/render_graph.h"

namespace Hybrid
{
    // Per-view materialization cache. The graph owns transient texture lifetime;
    // passes only resolve TextureViewHandle values by their declared graph name.
    class RenderGraphResourceRegistry
    {
    public:
        ~RenderGraphResourceRegistry();

        RenderGraphResourceRegistry() = default;
        RenderGraphResourceRegistry(const RenderGraphResourceRegistry&) = delete;
        RenderGraphResourceRegistry& operator=(const RenderGraphResourceRegistry&) = delete;

        void clearExternalResources();
        void importExternalResource(const std::string& name, TextureViewHandle view);
        RhiStatus prepare(IRenderDevice& device,
                          const RenderGraphCompileResult& graph,
                          uint32_t width,
                          uint32_t height);
        RhiResult<TextureViewHandle> texture(const std::string& name) const;
        void shutdown();

    private:
        struct OwnedTexture
        {
            TextureHandle texture;
            TextureViewHandle view;
            RhiTextureDesc desc;
        };

        static RhiResult<RhiTextureDesc> makeTextureDesc(const RenderGraphResourceDesc& resource,
                                                          uint32_t width,
                                                          uint32_t height);
        void destroyOwnedTexture(OwnedTexture& texture);

        IRenderDevice* m_device = nullptr;
        std::unordered_map<std::string, TextureViewHandle> m_external_resources;
        std::unordered_map<std::string, OwnedTexture> m_owned_resources;
    };
} // namespace Hybrid
