#pragma once

#include <memory>

#include "runtime/modules/render/rhi/rhi_types.h"

namespace Hybrid
{
    class IWindow;
    class ICommandList
    {
    public:
        virtual ~ICommandList() = default;
        virtual RhiStatus begin() = 0;
        virtual RhiStatus beginRenderPass(const RenderPassDesc& desc) = 0;
        virtual RhiStatus setViewport(const ViewportDesc& viewport) = 0;
        virtual RhiStatus bindPipeline(PipelineHandle pipeline) = 0;
        virtual RhiStatus bindTexture(TextureViewHandle texture,
                                      SamplerHandle sampler,
                                      ShaderBindingLocation location) = 0;
        virtual RhiStatus bindUniformBuffer(BufferHandle buffer,
                                            ShaderBindingLocation location) = 0;
        virtual RhiStatus bindVertexBuffer(BufferHandle buffer, uint32_t slot = 0) = 0;
        virtual RhiStatus bindIndexBuffer(BufferHandle buffer) = 0;
        virtual RhiStatus draw(uint32_t vertex_count, uint32_t first_vertex = 0) = 0;
        virtual RhiStatus drawIndexed(uint32_t index_count, uint32_t first_index = 0) = 0;
        virtual RhiStatus endRenderPass() = 0;
        virtual RhiStatus copyTexture(TextureViewHandle source, TextureViewHandle destination) = 0;
        virtual RhiStatus readbackTexture(TextureViewHandle texture, void* destination, size_t size) = 0;
        virtual RhiStatus end() = 0;
    };

    class ISwapchain
    {
    public:
        virtual ~ISwapchain() = default;
        virtual RhiResult<TextureViewHandle> acquire() = 0;
        virtual RhiStatus present() = 0;
        virtual RhiStatus resize(uint32_t width, uint32_t height) = 0;
        virtual RhiFormat format() const = 0;
        virtual uint32_t frameCount() const = 0;
    };

    class IRenderDevice
    {
    public:
        virtual ~IRenderDevice() = default;
        virtual GraphicsBackend backend() const = 0;
        virtual const RenderCapabilities& capabilities() const = 0;

        virtual RhiResult<BufferHandle> createBuffer(const BufferDesc& desc,
                                                     const void* initial_data = nullptr,
                                                     size_t initial_size = 0) = 0;
        virtual RhiStatus updateBuffer(BufferHandle handle, size_t offset,
                                       const void* data, size_t size) = 0;
        virtual RhiStatus destroyBuffer(BufferHandle handle) = 0;

        virtual RhiResult<TextureHandle> createTexture(const RhiTextureDesc& desc,
                                                       const void* initial_data = nullptr,
                                                       size_t initial_size = 0) = 0;
        virtual RhiResult<TextureViewHandle> createTextureView(TextureHandle texture) = 0;
        virtual RhiResult<RhiTextureDesc> textureDesc(TextureViewHandle view) const = 0;
        virtual RhiStatus destroyTextureView(TextureViewHandle handle) = 0;
        virtual RhiStatus destroyTexture(TextureHandle handle) = 0;

        virtual RhiResult<SamplerHandle> createSampler(const SamplerDesc& desc) = 0;
        virtual RhiStatus destroySampler(SamplerHandle handle) = 0;
        virtual RhiResult<ShaderHandle> createShaderModule(const ShaderModuleDesc& desc) = 0;
        virtual RhiStatus destroyShaderModule(ShaderHandle handle) = 0;
        virtual RhiResult<PipelineHandle> createGraphicsPipeline(const GraphicsPipelineDesc& desc) = 0;
        virtual RhiStatus destroyPipeline(PipelineHandle handle) = 0;

        virtual std::unique_ptr<ICommandList> createCommandList() = 0;
        virtual RhiStatus submit(ICommandList& command_list) = 0;
    };

    std::unique_ptr<IRenderDevice> CreateNullRenderDevice(bool validation_enabled = true);
    std::unique_ptr<IRenderDevice> CreateOpenGLRenderDevice(bool validation_enabled = true);
    std::unique_ptr<ISwapchain> CreateOpenGLSwapchain(IWindow& window);
} // namespace Hybrid
