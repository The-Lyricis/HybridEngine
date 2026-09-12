#include "runtime/modules/render/rhi/render_device.h"

#include <cstring>
#include <algorithm>
#include <vector>

#include "runtime/modules/render/rhi/handle_pool.h"

namespace Hybrid
{
    namespace
    {
        struct NullBuffer
        {
            BufferDesc desc;
            std::vector<uint8_t> bytes;
        };

        struct NullTexture
        {
            RhiTextureDesc desc;
            std::vector<uint8_t> bytes;
        };

        struct NullTextureView { TextureHandle texture; };
        struct NullSampler { SamplerDesc desc; };
        struct NullShader { ShaderModuleDesc desc; };
        struct NullPipeline { GraphicsPipelineDesc desc; };

        size_t textureByteSize(const RhiTextureDesc& desc)
        {
            const size_t component_size = desc.format == RhiFormat::R32Uint ||
                                          desc.format == RhiFormat::Depth24Stencil8 ||
                                          desc.format == RhiFormat::Depth32Float ? 4u : 1u;
            const size_t component_count = desc.format == RhiFormat::RGBA8Unorm ||
                                           desc.format == RhiFormat::RGBA8Srgb ||
                                           desc.format == RhiFormat::BGRA8Unorm ? 4u : 1u;
            return static_cast<size_t>(desc.width) * desc.height * desc.layers *
                   component_size * component_count;
        }

        class NullRenderDevice;

        class NullCommandList final : public ICommandList
        {
        public:
            explicit NullCommandList(NullRenderDevice& device) : m_device(device) {}

            RhiStatus begin() override;
            RhiStatus beginRenderPass(const RenderPassDesc& desc) override;
            RhiStatus setViewport(const ViewportDesc& viewport) override;
            RhiStatus bindPipeline(PipelineHandle pipeline) override;
            RhiStatus bindTexture(TextureViewHandle texture, SamplerHandle sampler, ShaderBindingLocation location) override;
            RhiStatus bindUniformBuffer(BufferHandle buffer, ShaderBindingLocation location) override;
            RhiStatus bindVertexBuffer(BufferHandle buffer, uint32_t slot) override;
            RhiStatus bindIndexBuffer(BufferHandle buffer) override;
            RhiStatus draw(uint32_t vertex_count, uint32_t first_vertex) override;
            RhiStatus drawIndexed(uint32_t index_count, uint32_t first_index) override;
            RhiStatus endRenderPass() override;
            RhiStatus copyTexture(TextureViewHandle source, TextureViewHandle destination) override;
            RhiStatus readbackTexture(TextureViewHandle texture, void* destination, size_t size) override;
            RhiStatus end() override;

            bool isExecutable() const { return m_state == State::Executable; }

        private:
            enum class State { Initial, Recording, InRenderPass, Executable };
            NullRenderDevice& m_device;
            State m_state = State::Initial;
            PipelineHandle m_pipeline;
            BufferHandle m_index_buffer;
        };

        class NullRenderDevice final : public IRenderDevice
        {
        public:
            explicit NullRenderDevice(bool validation_enabled)
                : m_validation_enabled(validation_enabled)
            {
                m_capabilities.max_texture_dimension_2d = 16384;
                m_capabilities.max_color_attachments = 8;
                m_capabilities.uniform_buffer_alignment = 256;
                m_capabilities.supports_compute = true;
                m_capabilities.supports_texture_arrays = true;
            }

            GraphicsBackend backend() const override { return GraphicsBackend::Null; }
            const RenderCapabilities& capabilities() const override { return m_capabilities; }

            RhiResult<BufferHandle> createBuffer(const BufferDesc& desc,
                                                 const void* initial_data,
                                                 size_t initial_size) override
            {
                if (desc.size == 0 || initial_size > desc.size || (initial_size > 0 && !initial_data))
                    return RhiResult<BufferHandle>::Failure(RhiErrorCode::InvalidArgument, "invalid buffer description or initial data");
                NullBuffer buffer{desc, std::vector<uint8_t>(desc.size)};
                if (initial_size > 0)
                    std::memcpy(buffer.bytes.data(), initial_data, initial_size);
                return RhiResult<BufferHandle>::Success(m_buffers.create(std::move(buffer)));
            }

            RhiStatus updateBuffer(BufferHandle handle, size_t offset,
                                   const void* data, size_t size) override
            {
                NullBuffer* buffer = m_buffers.get(handle);
                if (!buffer)
                    return invalidHandle("buffer");
                if (!data || offset > buffer->bytes.size() || size > buffer->bytes.size() - offset)
                    return RhiStatus::Failure(RhiErrorCode::OutOfBounds, "buffer update exceeds allocation");
                std::memcpy(buffer->bytes.data() + offset, data, size);
                return RhiStatus::Success();
            }

            RhiStatus destroyBuffer(BufferHandle handle) override
            {
                return m_buffers.destroy(handle) ? RhiStatus::Success() : invalidHandle("buffer");
            }

            RhiResult<TextureHandle> createTexture(const RhiTextureDesc& desc,
                                                   const void* initial_data,
                                                   size_t initial_size) override
            {
                if (desc.width == 0 || desc.height == 0 || desc.layers == 0 || desc.mip_levels == 0 ||
                    desc.format == RhiFormat::Unknown || (initial_size > 0 && !initial_data))
                    return RhiResult<TextureHandle>::Failure(RhiErrorCode::InvalidArgument, "invalid texture description or initial data");
                const size_t allocation_size = textureByteSize(desc);
                if (initial_size > allocation_size)
                    return RhiResult<TextureHandle>::Failure(RhiErrorCode::OutOfBounds, "texture initial data exceeds allocation");
                NullTexture texture{desc, std::vector<uint8_t>(allocation_size)};
                if (initial_size > 0)
                    std::memcpy(texture.bytes.data(), initial_data, initial_size);
                return RhiResult<TextureHandle>::Success(m_textures.create(std::move(texture)));
            }

            RhiResult<TextureViewHandle> createTextureView(TextureHandle texture) override
            {
                if (!m_textures.isValid(texture))
                    return RhiResult<TextureViewHandle>::Failure(RhiErrorCode::InvalidHandle, "texture handle is stale or invalid");
                return RhiResult<TextureViewHandle>::Success(m_texture_views.create({texture}));
            }

            RhiResult<RhiTextureDesc> textureDesc(TextureViewHandle handle) const override
            {
                const NullTexture* value = texture(handle);
                if (!value)
                    return RhiResult<RhiTextureDesc>::Failure(RhiErrorCode::InvalidHandle,
                                                               "texture view is stale or invalid");
                return RhiResult<RhiTextureDesc>::Success(value->desc);
            }

            RhiStatus destroyTextureView(TextureViewHandle handle) override
            {
                return m_texture_views.destroy(handle) ? RhiStatus::Success() : invalidHandle("texture view");
            }

            RhiStatus destroyTexture(TextureHandle handle) override
            {
                return m_textures.destroy(handle) ? RhiStatus::Success() : invalidHandle("texture");
            }

            RhiResult<SamplerHandle> createSampler(const SamplerDesc& desc) override
            {
                return RhiResult<SamplerHandle>::Success(m_samplers.create({desc}));
            }

            RhiStatus destroySampler(SamplerHandle handle) override
            {
                return m_samplers.destroy(handle) ? RhiStatus::Success() : invalidHandle("sampler");
            }

            RhiResult<ShaderHandle> createShaderModule(const ShaderModuleDesc& desc) override
            {
                if (desc.code.empty() || desc.entry_point.empty())
                    return RhiResult<ShaderHandle>::Failure(RhiErrorCode::InvalidArgument, "shader code and entry point are required");
                return RhiResult<ShaderHandle>::Success(m_shaders.create({desc}));
            }

            RhiStatus destroyShaderModule(ShaderHandle handle) override
            {
                return m_shaders.destroy(handle) ? RhiStatus::Success() : invalidHandle("shader");
            }

            RhiResult<PipelineHandle> createGraphicsPipeline(const GraphicsPipelineDesc& desc) override
            {
                const NullShader* vertex = m_shaders.get(desc.vertex_shader);
                const NullShader* fragment = m_shaders.get(desc.fragment_shader);
                if (!vertex || !fragment || vertex->desc.stage != RhiShaderStage::Vertex ||
                    fragment->desc.stage != RhiShaderStage::Fragment)
                {
                    return RhiResult<PipelineHandle>::Failure(RhiErrorCode::InvalidHandle,
                                                              "graphics pipeline requires valid vertex and fragment shaders");
                }
                const auto valid_bindings = [](const auto& bindings)
                {
                    for (size_t i = 0; i < bindings.size(); ++i)
                    {
                        if (bindings[i].name.empty() || bindings[i].location.set >= kRhiResourceSetCount)
                            return false;
                        for (size_t j = i + 1; j < bindings.size(); ++j)
                            if (bindings[i].location == bindings[j].location)
                                return false;
                    }
                    return true;
                };
                if (!valid_bindings(desc.texture_bindings) || !valid_bindings(desc.uniform_buffer_bindings))
                    return RhiResult<PipelineHandle>::Failure(RhiErrorCode::InvalidArgument,
                                                               "pipeline contains an invalid or duplicate resource binding");
                for (const TextureBindingDesc& texture_binding : desc.texture_bindings)
                    for (const UniformBufferBindingDesc& buffer_binding : desc.uniform_buffer_bindings)
                        if (texture_binding.location == buffer_binding.location)
                            return RhiResult<PipelineHandle>::Failure(RhiErrorCode::InvalidArgument,
                                                                       "pipeline resource bindings overlap across resource types");
                return RhiResult<PipelineHandle>::Success(m_pipelines.create({desc}));
            }

            RhiStatus destroyPipeline(PipelineHandle handle) override
            {
                return m_pipelines.destroy(handle) ? RhiStatus::Success() : invalidHandle("pipeline");
            }

            std::unique_ptr<ICommandList> createCommandList() override
            {
                return std::make_unique<NullCommandList>(*this);
            }

            RhiStatus submit(ICommandList& command_list) override
            {
                auto* null_commands = dynamic_cast<NullCommandList*>(&command_list);
                if (!null_commands || !null_commands->isExecutable())
                    return RhiStatus::Failure(RhiErrorCode::InvalidState, "command list is not executable or belongs to another device");
                return RhiStatus::Success();
            }

            bool hasBuffer(BufferHandle handle) const { return m_buffers.isValid(handle); }
            const NullBuffer* buffer(BufferHandle handle) const { return m_buffers.get(handle); }
            bool hasTextureView(TextureViewHandle handle) const
            {
                const NullTextureView* view = m_texture_views.get(handle);
                return view && m_textures.isValid(view->texture);
            }
            bool hasPipeline(PipelineHandle handle) const { return m_pipelines.isValid(handle); }
            const NullPipeline* pipeline(PipelineHandle handle) const { return m_pipelines.get(handle); }
            bool hasSampler(SamplerHandle handle) const { return m_samplers.isValid(handle); }
            NullTexture* texture(TextureViewHandle handle)
            {
                NullTextureView* view = m_texture_views.get(handle);
                return view ? m_textures.get(view->texture) : nullptr;
            }
            const NullTexture* texture(TextureViewHandle handle) const
            {
                const NullTextureView* view = m_texture_views.get(handle);
                return view ? m_textures.get(view->texture) : nullptr;
            }

        private:
            static RhiStatus invalidHandle(const char* resource)
            {
                return RhiStatus::Failure(RhiErrorCode::InvalidHandle,
                                          std::string(resource) + " handle is stale or invalid");
            }

            bool m_validation_enabled = true;
            RenderCapabilities m_capabilities{};
            HandlePool<BufferHandle, NullBuffer> m_buffers;
            HandlePool<TextureHandle, NullTexture> m_textures;
            HandlePool<TextureViewHandle, NullTextureView> m_texture_views;
            HandlePool<SamplerHandle, NullSampler> m_samplers;
            HandlePool<ShaderHandle, NullShader> m_shaders;
            HandlePool<PipelineHandle, NullPipeline> m_pipelines;
        };

        RhiStatus NullCommandList::begin()
        {
            if (m_state != State::Initial && m_state != State::Executable)
                return RhiStatus::Failure(RhiErrorCode::InvalidState, "command list is already recording");
            m_state = State::Recording;
            m_pipeline = {};
            m_index_buffer = {};
            return RhiStatus::Success();
        }

        RhiStatus NullCommandList::beginRenderPass(const RenderPassDesc& desc)
        {
            if (m_state != State::Recording)
                return RhiStatus::Failure(RhiErrorCode::InvalidState, "beginRenderPass requires recording state");
            if (desc.colors.empty() && !desc.has_depth)
                return RhiStatus::Failure(RhiErrorCode::InvalidArgument, "render pass has no attachments");
            for (const ColorAttachmentDesc& color : desc.colors)
                if (!m_device.hasTextureView(color.texture))
                    return RhiStatus::Failure(RhiErrorCode::InvalidHandle, "render pass color view is invalid");
            if (desc.has_depth && !m_device.hasTextureView(desc.depth.texture))
                return RhiStatus::Failure(RhiErrorCode::InvalidHandle, "render pass depth view is invalid");
            m_state = State::InRenderPass;
            return RhiStatus::Success();
        }

        RhiStatus NullCommandList::setViewport(const ViewportDesc& viewport)
        {
            if (m_state != State::InRenderPass)
                return RhiStatus::Failure(RhiErrorCode::InvalidState, "viewport requires an active render pass");
            if (viewport.width <= 0.0f || viewport.height <= 0.0f)
                return RhiStatus::Failure(RhiErrorCode::InvalidArgument, "viewport dimensions must be positive");
            return RhiStatus::Success();
        }

        RhiStatus NullCommandList::bindPipeline(PipelineHandle pipeline)
        {
            if (m_state != State::InRenderPass || !m_device.hasPipeline(pipeline))
                return RhiStatus::Failure(RhiErrorCode::InvalidHandle, "pipeline is invalid or no render pass is active");
            m_pipeline = pipeline;
            return RhiStatus::Success();
        }

        RhiStatus NullCommandList::bindTexture(TextureViewHandle texture,
                                               SamplerHandle sampler,
                                               ShaderBindingLocation location)
        {
            const NullPipeline* pipeline = m_device.pipeline(m_pipeline);
            if (m_state != State::InRenderPass || !pipeline ||
                !m_device.hasTextureView(texture) || !m_device.hasSampler(sampler))
                return RhiStatus::Failure(RhiErrorCode::InvalidHandle, "texture, sampler, or pipeline is invalid");
            const auto found = std::find_if(pipeline->desc.texture_bindings.begin(),
                                            pipeline->desc.texture_bindings.end(),
                                            [location](const TextureBindingDesc& value) { return value.location == location; });
            if (found == pipeline->desc.texture_bindings.end())
                return RhiStatus::Failure(RhiErrorCode::InvalidArgument, "texture binding is not declared by the pipeline");
            return RhiStatus::Success();
        }

        RhiStatus NullCommandList::bindUniformBuffer(BufferHandle buffer,
                                                     ShaderBindingLocation location)
        {
            const NullPipeline* pipeline = m_device.pipeline(m_pipeline);
            const NullBuffer* value = m_device.buffer(buffer);
            if (m_state != State::InRenderPass || !pipeline || !value ||
                value->desc.usage != RhiBufferUsage::Uniform)
                return RhiStatus::Failure(RhiErrorCode::InvalidHandle, "uniform buffer or pipeline is invalid");
            const auto found = std::find_if(pipeline->desc.uniform_buffer_bindings.begin(),
                                            pipeline->desc.uniform_buffer_bindings.end(),
                                            [location](const UniformBufferBindingDesc& item) { return item.location == location; });
            if (found == pipeline->desc.uniform_buffer_bindings.end())
                return RhiStatus::Failure(RhiErrorCode::InvalidArgument, "uniform buffer binding is not declared by the pipeline");
            return RhiStatus::Success();
        }

        RhiStatus NullCommandList::bindVertexBuffer(BufferHandle buffer, uint32_t)
        {
            if (m_state != State::InRenderPass || !m_device.hasBuffer(buffer))
                return RhiStatus::Failure(RhiErrorCode::InvalidHandle, "vertex buffer is invalid or no render pass is active");
            return RhiStatus::Success();
        }

        RhiStatus NullCommandList::bindIndexBuffer(BufferHandle buffer)
        {
            if (m_state != State::InRenderPass || !m_device.hasBuffer(buffer))
                return RhiStatus::Failure(RhiErrorCode::InvalidHandle, "index buffer is invalid or no render pass is active");
            m_index_buffer = buffer;
            return RhiStatus::Success();
        }

        RhiStatus NullCommandList::draw(uint32_t vertex_count, uint32_t)
        {
            if (m_state != State::InRenderPass || !m_device.hasPipeline(m_pipeline) || vertex_count == 0)
                return RhiStatus::Failure(RhiErrorCode::InvalidState, "draw requires a pipeline and non-zero vertex count");
            return RhiStatus::Success();
        }

        RhiStatus NullCommandList::drawIndexed(uint32_t index_count, uint32_t)
        {
            if (m_state != State::InRenderPass || !m_device.hasPipeline(m_pipeline) ||
                !m_device.hasBuffer(m_index_buffer) || index_count == 0)
                return RhiStatus::Failure(RhiErrorCode::InvalidState, "drawIndexed requires pipeline, index buffer, and non-zero index count");
            return RhiStatus::Success();
        }

        RhiStatus NullCommandList::endRenderPass()
        {
            if (m_state != State::InRenderPass)
                return RhiStatus::Failure(RhiErrorCode::InvalidState, "no render pass is active");
            m_state = State::Recording;
            return RhiStatus::Success();
        }

        RhiStatus NullCommandList::copyTexture(TextureViewHandle source, TextureViewHandle destination)
        {
            if (m_state != State::Recording)
                return RhiStatus::Failure(RhiErrorCode::InvalidState, "copyTexture requires recording state outside a render pass");
            const NullTexture* src = m_device.texture(source);
            NullTexture* dst = m_device.texture(destination);
            if (!src || !dst)
                return RhiStatus::Failure(RhiErrorCode::InvalidHandle, "copy texture view is stale or invalid");
            if (src->desc.width != dst->desc.width || src->desc.height != dst->desc.height ||
                src->desc.format != dst->desc.format)
                return RhiStatus::Failure(RhiErrorCode::InvalidArgument, "copy textures must have matching dimensions and format");
            dst->bytes = src->bytes;
            return RhiStatus::Success();
        }

        RhiStatus NullCommandList::readbackTexture(TextureViewHandle handle, void* destination, size_t size)
        {
            if (m_state != State::Recording)
                return RhiStatus::Failure(RhiErrorCode::InvalidState, "readback requires recording state outside a render pass");
            const NullTexture* texture = m_device.texture(handle);
            if (!texture)
                return RhiStatus::Failure(RhiErrorCode::InvalidHandle, "texture view is stale or invalid");
            const size_t required = textureByteSize(texture->desc);
            if (!destination || size < required)
                return RhiStatus::Failure(RhiErrorCode::OutOfBounds, "readback destination is too small");
            std::memset(destination, 0, required);
            return RhiStatus::Success();
        }

        RhiStatus NullCommandList::end()
        {
            if (m_state != State::Recording)
                return RhiStatus::Failure(RhiErrorCode::InvalidState, "command list must be recording outside a render pass");
            m_state = State::Executable;
            return RhiStatus::Success();
        }
    } // namespace

    std::unique_ptr<IRenderDevice> CreateNullRenderDevice(bool validation_enabled)
    {
        return std::make_unique<NullRenderDevice>(validation_enabled);
    }
} // namespace Hybrid
