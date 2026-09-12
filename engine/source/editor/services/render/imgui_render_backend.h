#pragma once

#include <memory>
#include <cstdint>
#include <string>

#include "runtime/core/platform/graphics_backend.h"
#include "runtime/modules/render/public/texture.h"
#include "runtime/modules/render/rhi/rhi_handles.h"

namespace Hybrid
{
    class IWindow;
    class IRenderDevice;

    // Owns ImGui context and backend frame/render lifecycle. Editor layers do not
    // depend on GLFW or a concrete graphics API through this interface.
    class IImGuiRenderBackend
    {
    public:
        virtual ~IImGuiRenderBackend() = default;
        virtual bool initialize(IWindow& window, std::string& error) = 0;
        virtual void newFrame() = 0;
        virtual void render() = 0;
        virtual uint64_t registerTexture(const TexturePtr& texture, std::string& error) = 0;
        virtual uint64_t registerTextureView(IRenderDevice& device,
                                             TextureViewHandle view,
                                             std::string& error) = 0;
        virtual void unregisterTexture(uint64_t texture_id) = 0;
        virtual uint32_t maxTextureDimension2D() const = 0;
        virtual void shutdown() = 0;
    };

    std::unique_ptr<IImGuiRenderBackend> CreateImGuiRenderBackend(GraphicsBackend backend);
} // namespace Hybrid
