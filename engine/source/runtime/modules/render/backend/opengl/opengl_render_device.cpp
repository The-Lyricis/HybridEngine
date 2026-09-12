#include "runtime/modules/render/rhi/render_device.h"
#include "runtime/modules/render/backend/opengl/opengl_render_device_bridge.h"

#include <algorithm>
#include <cstring>
#include <iterator>
#include <string>
#include <vector>

#include <glad/gl.h>

#include "runtime/modules/render/rhi/handle_pool.h"
#include "runtime/core/platform/window.h"

namespace Hybrid
{
    namespace
    {
        struct OpenGLBuffer
        {
            BufferDesc desc;
            GLuint id = 0;
            GLenum target = GL_ARRAY_BUFFER;
        };

        struct OpenGLTexture
        {
            RhiTextureDesc desc;
            GLuint id = 0;
            GLenum target = GL_TEXTURE_2D;
        };

        struct OpenGLTextureView { TextureHandle texture; };
        struct OpenGLSampler { SamplerDesc desc; GLuint id = 0; };
        struct OpenGLShader { ShaderModuleDesc desc; GLuint id = 0; };
        struct OpenGLPipeline { GraphicsPipelineDesc desc; GLuint program = 0; };

        class OpenGLSwapchain final : public ISwapchain
        {
        public:
            explicit OpenGLSwapchain(IWindow& window) : m_window(&window)
            {
                const FramebufferSize size = window.framebufferSize();
                m_width = size.width;
                m_height = size.height;
            }

            RhiResult<TextureViewHandle> acquire() override
            {
                return RhiResult<TextureViewHandle>::Failure(
                    RhiErrorCode::UnsupportedBackend,
                    "OpenGL default framebuffer is presented directly and has no texture view");
            }

            RhiStatus present() override
            {
                if (!m_window)
                    return RhiStatus::Failure(RhiErrorCode::InvalidState, "swapchain window is unavailable");
                m_window->swapBuffers();
                return RhiStatus::Success();
            }

            RhiStatus resize(uint32_t width, uint32_t height) override
            {
                if (width == 0 || height == 0)
                    return RhiStatus::Failure(RhiErrorCode::InvalidArgument, "swapchain dimensions must be non-zero");
                m_width = width;
                m_height = height;
                return RhiStatus::Success();
            }

            RhiFormat format() const override { return RhiFormat::BGRA8Unorm; }
            uint32_t frameCount() const override { return 2; }

        private:
            IWindow* m_window = nullptr;
            uint32_t m_width = 0;
            uint32_t m_height = 0;
        };

        GLenum bufferTarget(RhiBufferUsage usage)
        {
            switch (usage)
            {
            case RhiBufferUsage::Vertex:  return GL_ARRAY_BUFFER;
            case RhiBufferUsage::Index:   return GL_ELEMENT_ARRAY_BUFFER;
            case RhiBufferUsage::Uniform: return GL_UNIFORM_BUFFER;
            case RhiBufferUsage::Staging: return GL_COPY_READ_BUFFER;
            case RhiBufferUsage::Storage: return 0;
            }
            return 0;
        }

        GLenum shaderStage(RhiShaderStage stage)
        {
            switch (stage)
            {
            case RhiShaderStage::Vertex:   return GL_VERTEX_SHADER;
            case RhiShaderStage::Fragment: return GL_FRAGMENT_SHADER;
            case RhiShaderStage::Compute:  return 0;
            }
            return 0;
        }

        bool textureFormat(RhiFormat format, GLint& internal, GLenum& external, GLenum& type)
        {
            switch (format)
            {
            case RhiFormat::R8Unorm:
                internal = GL_R8; external = GL_RED; type = GL_UNSIGNED_BYTE; return true;
            case RhiFormat::RGBA8Unorm:
                internal = GL_RGBA8; external = GL_RGBA; type = GL_UNSIGNED_BYTE; return true;
            case RhiFormat::BGRA8Unorm:
                internal = GL_RGBA8; external = GL_BGRA; type = GL_UNSIGNED_BYTE; return true;
            case RhiFormat::RGBA8Srgb:
                internal = GL_SRGB8_ALPHA8; external = GL_RGBA; type = GL_UNSIGNED_BYTE; return true;
            case RhiFormat::R32Uint:
                internal = GL_R32UI; external = GL_RED_INTEGER; type = GL_UNSIGNED_INT; return true;
            case RhiFormat::Depth24Stencil8:
                internal = GL_DEPTH24_STENCIL8; external = GL_DEPTH_STENCIL; type = GL_UNSIGNED_INT_24_8; return true;
            case RhiFormat::Depth32Float:
                internal = GL_DEPTH_COMPONENT32F; external = GL_DEPTH_COMPONENT; type = GL_FLOAT; return true;
            case RhiFormat::Unknown:
                return false;
            }
            return false;
        }

        class OpenGLRenderDevice;

        class OpenGLCommandList final : public ICommandList
        {
        public:
            explicit OpenGLCommandList(OpenGLRenderDevice& device);
            ~OpenGLCommandList() override;

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

            bool executable() const { return m_state == State::Executable; }

        private:
            enum class State { Initial, Recording, InRenderPass, Executable };
            OpenGLRenderDevice& m_device;
            State m_state = State::Initial;
            GLuint m_framebuffer = 0;
            GLuint m_copy_framebuffer = 0;
            GLuint m_vertex_array = 0;
            uint32_t m_attached_colors = 0;
            PipelineHandle m_pipeline;
            BufferHandle m_index_buffer;
        };

        class OpenGLRenderDevice final : public IRenderDevice
        {
        public:
            explicit OpenGLRenderDevice(bool validation_enabled)
                : m_validation_enabled(validation_enabled)
            {
                GLint value = 1;
                glGetIntegerv(GL_MAX_TEXTURE_SIZE, &value);
                m_capabilities.max_texture_dimension_2d = static_cast<uint32_t>(std::max(value, 1));
                glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS, &value);
                m_capabilities.max_color_attachments = static_cast<uint32_t>(std::max(value, 1));
                glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &value);
                m_capabilities.uniform_buffer_alignment = static_cast<uint32_t>(std::max(value, 1));
                m_capabilities.supports_compute = false;
                m_capabilities.supports_texture_arrays = true;
            }

            ~OpenGLRenderDevice() override
            {
                m_pipelines.forEachAlive([](OpenGLPipeline& value) { if (value.program) glDeleteProgram(value.program); });
                m_shaders.forEachAlive([](OpenGLShader& value) { if (value.id) glDeleteShader(value.id); });
                m_samplers.forEachAlive([](OpenGLSampler& value) { if (value.id) glDeleteSamplers(1, &value.id); });
                m_textures.forEachAlive([](OpenGLTexture& value) { if (value.id) glDeleteTextures(1, &value.id); });
                m_buffers.forEachAlive([](OpenGLBuffer& value) { if (value.id) glDeleteBuffers(1, &value.id); });
            }

            GraphicsBackend backend() const override { return GraphicsBackend::OpenGL; }
            const RenderCapabilities& capabilities() const override { return m_capabilities; }

            RhiResult<BufferHandle> createBuffer(const BufferDesc& desc, const void* data, size_t size) override
            {
                const GLenum target = bufferTarget(desc.usage);
                if (desc.size == 0 || size > desc.size || (size > 0 && !data))
                    return RhiResult<BufferHandle>::Failure(RhiErrorCode::InvalidArgument, "invalid buffer description or initial data");
                if (target == 0)
                    return RhiResult<BufferHandle>::Failure(RhiErrorCode::UnsupportedBackend, "OpenGL 4.1 does not support storage buffers");

                GLuint id = 0;
                glGenBuffers(1, &id);
                glBindBuffer(target, id);
                glBufferData(target, static_cast<GLsizeiptr>(desc.size), data,
                             desc.memory == RhiMemoryUsage::GPUOnly ? GL_STATIC_DRAW : GL_DYNAMIC_DRAW);
                glBindBuffer(target, 0);
                if (!id)
                    return RhiResult<BufferHandle>::Failure(RhiErrorCode::ResourceCreationFailed, "glGenBuffers failed");
                return RhiResult<BufferHandle>::Success(m_buffers.create({desc, id, target}));
            }

            RhiStatus updateBuffer(BufferHandle handle, size_t offset, const void* data, size_t size) override
            {
                OpenGLBuffer* buffer = m_buffers.get(handle);
                if (!buffer)
                    return invalidHandle("buffer");
                if (!data || offset > buffer->desc.size || size > buffer->desc.size - offset)
                    return RhiStatus::Failure(RhiErrorCode::OutOfBounds, "buffer update exceeds allocation");
                glBindBuffer(buffer->target, buffer->id);
                glBufferSubData(buffer->target, static_cast<GLintptr>(offset), static_cast<GLsizeiptr>(size), data);
                glBindBuffer(buffer->target, 0);
                return RhiStatus::Success();
            }

            RhiStatus destroyBuffer(BufferHandle handle) override
            {
                OpenGLBuffer* buffer = m_buffers.get(handle);
                if (!buffer)
                    return invalidHandle("buffer");
                glDeleteBuffers(1, &buffer->id);
                return m_buffers.destroy(handle) ? RhiStatus::Success() : invalidHandle("buffer");
            }

            RhiResult<TextureHandle> createTexture(const RhiTextureDesc& desc, const void* data, size_t size) override
            {
                if (desc.type != RhiTextureType::Texture2D)
                    return RhiResult<TextureHandle>::Failure(RhiErrorCode::UnsupportedBackend, "initial OpenGL RHI supports Texture2D only");
                if (desc.width == 0 || desc.height == 0 || desc.layers != 1 || desc.mip_levels != 1 || (size > 0 && !data))
                    return RhiResult<TextureHandle>::Failure(RhiErrorCode::InvalidArgument, "invalid texture description or initial data");
                GLint internal = 0;
                GLenum external = 0;
                GLenum type = 0;
                if (!textureFormat(desc.format, internal, external, type))
                    return RhiResult<TextureHandle>::Failure(RhiErrorCode::InvalidArgument, "unsupported texture format");

                GLuint id = 0;
                glGenTextures(1, &id);
                glBindTexture(GL_TEXTURE_2D, id);
                const bool linear_filter = desc.format == RhiFormat::RGBA8Unorm ||
                                           desc.format == RhiFormat::BGRA8Unorm ||
                                           desc.format == RhiFormat::RGBA8Srgb;
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, linear_filter ? GL_LINEAR : GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, linear_filter ? GL_LINEAR : GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                if (desc.format == RhiFormat::Depth24Stencil8 || desc.format == RhiFormat::Depth32Float)
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);
                glTexImage2D(GL_TEXTURE_2D, 0, internal, static_cast<GLsizei>(desc.width),
                             static_cast<GLsizei>(desc.height), 0, external, type, data);
                glBindTexture(GL_TEXTURE_2D, 0);
                if (!id)
                    return RhiResult<TextureHandle>::Failure(RhiErrorCode::ResourceCreationFailed, "glGenTextures failed");
                return RhiResult<TextureHandle>::Success(m_textures.create({desc, id, GL_TEXTURE_2D}));
            }

            RhiResult<TextureViewHandle> createTextureView(TextureHandle texture) override
            {
                if (!m_textures.isValid(texture))
                    return RhiResult<TextureViewHandle>::Failure(RhiErrorCode::InvalidHandle, "texture handle is stale or invalid");
                return RhiResult<TextureViewHandle>::Success(m_views.create({texture}));
            }

            RhiResult<RhiTextureDesc> textureDesc(TextureViewHandle handle) const override
            {
                const OpenGLTexture* value = texture(handle);
                if (!value)
                    return RhiResult<RhiTextureDesc>::Failure(RhiErrorCode::InvalidHandle,
                                                               "texture view is stale or invalid");
                return RhiResult<RhiTextureDesc>::Success(value->desc);
            }

            RhiStatus destroyTextureView(TextureViewHandle handle) override
            {
                return m_views.destroy(handle) ? RhiStatus::Success() : invalidHandle("texture view");
            }

            RhiStatus destroyTexture(TextureHandle handle) override
            {
                OpenGLTexture* texture = m_textures.get(handle);
                if (!texture)
                    return invalidHandle("texture");
                glDeleteTextures(1, &texture->id);
                return m_textures.destroy(handle) ? RhiStatus::Success() : invalidHandle("texture");
            }

            RhiResult<SamplerHandle> createSampler(const SamplerDesc& desc) override
            {
                GLuint id = 0;
                glGenSamplers(1, &id);
                glSamplerParameteri(id, GL_TEXTURE_MIN_FILTER, desc.linear_filter ? GL_LINEAR : GL_NEAREST);
                glSamplerParameteri(id, GL_TEXTURE_MAG_FILTER, desc.linear_filter ? GL_LINEAR : GL_NEAREST);
                const GLint wrap = desc.clamp_to_edge ? GL_CLAMP_TO_EDGE : GL_REPEAT;
                glSamplerParameteri(id, GL_TEXTURE_WRAP_S, wrap);
                glSamplerParameteri(id, GL_TEXTURE_WRAP_T, wrap);
                if (!id)
                    return RhiResult<SamplerHandle>::Failure(RhiErrorCode::ResourceCreationFailed, "glGenSamplers failed");
                return RhiResult<SamplerHandle>::Success(m_samplers.create({desc, id}));
            }

            RhiStatus destroySampler(SamplerHandle handle) override
            {
                OpenGLSampler* sampler = m_samplers.get(handle);
                if (!sampler)
                    return invalidHandle("sampler");
                glDeleteSamplers(1, &sampler->id);
                return m_samplers.destroy(handle) ? RhiStatus::Success() : invalidHandle("sampler");
            }

            RhiResult<ShaderHandle> createShaderModule(const ShaderModuleDesc& desc) override
            {
                const GLenum stage = shaderStage(desc.stage);
                if (stage == 0)
                    return RhiResult<ShaderHandle>::Failure(RhiErrorCode::UnsupportedBackend, "OpenGL 4.1 compute shaders are unsupported");
                if (desc.code.empty() || desc.entry_point.empty())
                    return RhiResult<ShaderHandle>::Failure(RhiErrorCode::InvalidArgument, "shader code and entry point are required");
                const GLuint shader = glCreateShader(stage);
                const char* source = reinterpret_cast<const char*>(desc.code.data());
                const GLint length = static_cast<GLint>(desc.code.size());
                glShaderSource(shader, 1, &source, &length);
                glCompileShader(shader);
                GLint compiled = GL_FALSE;
                glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
                if (compiled == GL_FALSE)
                {
                    GLint log_length = 0;
                    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_length);
                    std::string log(static_cast<size_t>(std::max(log_length, 1)), '\0');
                    glGetShaderInfoLog(shader, log_length, nullptr, log.data());
                    glDeleteShader(shader);
                    return RhiResult<ShaderHandle>::Failure(RhiErrorCode::ShaderCompilationFailed, log);
                }
                return RhiResult<ShaderHandle>::Success(m_shaders.create({desc, shader}));
            }

            RhiStatus destroyShaderModule(ShaderHandle handle) override
            {
                OpenGLShader* shader = m_shaders.get(handle);
                if (!shader)
                    return invalidHandle("shader");
                glDeleteShader(shader->id);
                return m_shaders.destroy(handle) ? RhiStatus::Success() : invalidHandle("shader");
            }

            RhiResult<PipelineHandle> createGraphicsPipeline(const GraphicsPipelineDesc& desc) override
            {
                OpenGLShader* vertex = m_shaders.get(desc.vertex_shader);
                OpenGLShader* fragment = m_shaders.get(desc.fragment_shader);
                if (!vertex || !fragment || vertex->desc.stage != RhiShaderStage::Vertex ||
                    fragment->desc.stage != RhiShaderStage::Fragment)
                    return RhiResult<PipelineHandle>::Failure(RhiErrorCode::InvalidHandle, "pipeline requires vertex and fragment shaders");

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
                    return RhiResult<PipelineHandle>::Failure(
                        RhiErrorCode::InvalidArgument,
                        "pipeline contains an invalid or duplicate resource binding");
                for (const TextureBindingDesc& texture_binding : desc.texture_bindings)
                    for (const UniformBufferBindingDesc& buffer_binding : desc.uniform_buffer_bindings)
                        if (texture_binding.location == buffer_binding.location)
                            return RhiResult<PipelineHandle>::Failure(
                                RhiErrorCode::InvalidArgument,
                                "pipeline resource bindings overlap across resource types");

                const GLuint program = glCreateProgram();
                glAttachShader(program, vertex->id);
                glAttachShader(program, fragment->id);
                glLinkProgram(program);
                GLint linked = GL_FALSE;
                glGetProgramiv(program, GL_LINK_STATUS, &linked);
                if (linked == GL_FALSE)
                {
                    GLint log_length = 0;
                    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &log_length);
                    std::string log(static_cast<size_t>(std::max(log_length, 1)), '\0');
                    glGetProgramInfoLog(program, log_length, nullptr, log.data());
                    glDeleteProgram(program);
                    return RhiResult<PipelineHandle>::Failure(RhiErrorCode::ResourceCreationFailed, log);
                }

                glUseProgram(program);
                for (size_t binding_index = 0; binding_index < desc.texture_bindings.size(); ++binding_index)
                {
                    const TextureBindingDesc& binding = desc.texture_bindings[binding_index];
                    const GLint location = glGetUniformLocation(program, binding.name.c_str());
                    if (location < 0)
                    {
                        glUseProgram(0);
                        glDeleteProgram(program);
                        return RhiResult<PipelineHandle>::Failure(
                            RhiErrorCode::ResourceCreationFailed,
                            "pipeline texture binding was not found: " + binding.name);
                    }
                    glUniform1i(location, static_cast<GLint>(binding_index));
                }
                for (size_t binding_index = 0; binding_index < desc.uniform_buffer_bindings.size(); ++binding_index)
                {
                    const UniformBufferBindingDesc& binding = desc.uniform_buffer_bindings[binding_index];
                    const GLuint block = glGetUniformBlockIndex(program, binding.name.c_str());
                    if (block == GL_INVALID_INDEX)
                    {
                        glUseProgram(0);
                        glDeleteProgram(program);
                        return RhiResult<PipelineHandle>::Failure(
                            RhiErrorCode::ResourceCreationFailed,
                            "pipeline uniform block was not found: " + binding.name);
                    }
                    glUniformBlockBinding(program, block, static_cast<GLuint>(binding_index));
                }
                glUseProgram(0);
                return RhiResult<PipelineHandle>::Success(m_pipelines.create({desc, program}));
            }

            RhiStatus destroyPipeline(PipelineHandle handle) override
            {
                OpenGLPipeline* pipeline = m_pipelines.get(handle);
                if (!pipeline)
                    return invalidHandle("pipeline");
                glDeleteProgram(pipeline->program);
                return m_pipelines.destroy(handle) ? RhiStatus::Success() : invalidHandle("pipeline");
            }

            std::unique_ptr<ICommandList> createCommandList() override
            {
                return std::make_unique<OpenGLCommandList>(*this);
            }

            RhiStatus submit(ICommandList& command_list) override
            {
                auto* commands = dynamic_cast<OpenGLCommandList*>(&command_list);
                if (!commands || !commands->executable())
                    return RhiStatus::Failure(RhiErrorCode::InvalidState, "command list is not executable or belongs to another backend");
                return RhiStatus::Success();
            }

            OpenGLBuffer* buffer(BufferHandle handle) { return m_buffers.get(handle); }
            OpenGLTexture* texture(TextureViewHandle handle)
            {
                OpenGLTextureView* view = m_views.get(handle);
                return view ? m_textures.get(view->texture) : nullptr;
            }
            const OpenGLTexture* texture(TextureViewHandle handle) const
            {
                const OpenGLTextureView* view = m_views.get(handle);
                return view ? m_textures.get(view->texture) : nullptr;
            }
            OpenGLPipeline* pipeline(PipelineHandle handle) { return m_pipelines.get(handle); }
            OpenGLSampler* sampler(SamplerHandle handle) { return m_samplers.get(handle); }

        private:
            static RhiStatus invalidHandle(const char* resource)
            {
                return RhiStatus::Failure(RhiErrorCode::InvalidHandle,
                                          std::string(resource) + " handle is stale or invalid");
            }

            bool m_validation_enabled = true;
            RenderCapabilities m_capabilities{};
            HandlePool<BufferHandle, OpenGLBuffer> m_buffers;
            HandlePool<TextureHandle, OpenGLTexture> m_textures;
            HandlePool<TextureViewHandle, OpenGLTextureView> m_views;
            HandlePool<SamplerHandle, OpenGLSampler> m_samplers;
            HandlePool<ShaderHandle, OpenGLShader> m_shaders;
            HandlePool<PipelineHandle, OpenGLPipeline> m_pipelines;
        };

        RhiStatus invalidState(const char* message)
        {
            return RhiStatus::Failure(RhiErrorCode::InvalidState, message);
        }

        OpenGLCommandList::OpenGLCommandList(OpenGLRenderDevice& device) : m_device(device)
        {
            glGenFramebuffers(1, &m_framebuffer);
            glGenFramebuffers(1, &m_copy_framebuffer);
            glGenVertexArrays(1, &m_vertex_array);
        }

        OpenGLCommandList::~OpenGLCommandList()
        {
            if (m_vertex_array) glDeleteVertexArrays(1, &m_vertex_array);
            if (m_copy_framebuffer) glDeleteFramebuffers(1, &m_copy_framebuffer);
            if (m_framebuffer) glDeleteFramebuffers(1, &m_framebuffer);
        }

        RhiStatus OpenGLCommandList::begin()
        {
            if (m_state != State::Initial && m_state != State::Executable)
                return invalidState("command list is already recording");
            m_state = State::Recording;
            m_pipeline = {};
            m_index_buffer = {};
            return RhiStatus::Success();
        }

        RhiStatus OpenGLCommandList::beginRenderPass(const RenderPassDesc& desc)
        {
            if (m_state != State::Recording)
                return invalidState("beginRenderPass requires recording state");
            if (desc.colors.empty() && !desc.has_depth)
                return RhiStatus::Failure(RhiErrorCode::InvalidArgument, "render pass has no attachments");

            glBindFramebuffer(GL_FRAMEBUFFER, m_framebuffer);
            std::vector<GLenum> draw_buffers;
            draw_buffers.reserve(desc.colors.size());
            for (uint32_t index = 0; index < desc.colors.size(); ++index)
            {
                OpenGLTexture* texture = m_device.texture(desc.colors[index].texture);
                if (!texture)
                {
                    glBindFramebuffer(GL_FRAMEBUFFER, 0);
                    return RhiStatus::Failure(RhiErrorCode::InvalidHandle, "render pass color view is invalid");
                }
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + index,
                                       texture->target, texture->id, 0);
                draw_buffers.push_back(GL_COLOR_ATTACHMENT0 + index);
            }
            for (uint32_t index = static_cast<uint32_t>(desc.colors.size()); index < m_attached_colors; ++index)
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + index, GL_TEXTURE_2D, 0, 0);
            m_attached_colors = static_cast<uint32_t>(desc.colors.size());
            if (draw_buffers.empty())
                glDrawBuffer(GL_NONE);
            else
                glDrawBuffers(static_cast<GLsizei>(draw_buffers.size()), draw_buffers.data());

            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, 0, 0);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, 0, 0);
            if (desc.has_depth)
            {
                OpenGLTexture* depth = m_device.texture(desc.depth.texture);
                if (!depth)
                {
                    glBindFramebuffer(GL_FRAMEBUFFER, 0);
                    return RhiStatus::Failure(RhiErrorCode::InvalidHandle, "render pass depth view is invalid");
                }
                const GLenum attachment = depth->desc.format == RhiFormat::Depth24Stencil8
                    ? GL_DEPTH_STENCIL_ATTACHMENT
                    : GL_DEPTH_ATTACHMENT;
                glFramebufferTexture2D(GL_FRAMEBUFFER, attachment, depth->target, depth->id, 0);
            }
            if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            {
                glBindFramebuffer(GL_FRAMEBUFFER, 0);
                return RhiStatus::Failure(RhiErrorCode::ResourceCreationFailed, "render pass framebuffer is incomplete");
            }

            for (uint32_t index = 0; index < desc.colors.size(); ++index)
            {
                if (!desc.colors[index].clear)
                    continue;
                OpenGLTexture* texture = m_device.texture(desc.colors[index].texture);
                if (texture && texture->desc.format == RhiFormat::R32Uint)
                {
                    const GLuint clear[4] = {};
                    glClearBufferuiv(GL_COLOR, static_cast<GLint>(index), clear);
                }
                else
                {
                    glClearBufferfv(GL_COLOR, static_cast<GLint>(index), desc.colors[index].clear_color);
                }
            }
            if (desc.has_depth && desc.depth.clear)
                glClearBufferfv(GL_DEPTH, 0, &desc.depth.clear_depth);
            m_state = State::InRenderPass;
            return RhiStatus::Success();
        }

        RhiStatus OpenGLCommandList::setViewport(const ViewportDesc& viewport)
        {
            if (m_state != State::InRenderPass)
                return invalidState("viewport requires an active render pass");
            if (viewport.width <= 0.0f || viewport.height <= 0.0f)
                return RhiStatus::Failure(RhiErrorCode::InvalidArgument, "viewport dimensions must be positive");
            glViewport(static_cast<GLint>(viewport.x), static_cast<GLint>(viewport.y),
                       static_cast<GLsizei>(viewport.width), static_cast<GLsizei>(viewport.height));
            glDepthRange(viewport.min_depth, viewport.max_depth);
            return RhiStatus::Success();
        }

        RhiStatus OpenGLCommandList::bindPipeline(PipelineHandle handle)
        {
            OpenGLPipeline* pipeline = m_device.pipeline(handle);
            if (m_state != State::InRenderPass || !pipeline)
                return RhiStatus::Failure(RhiErrorCode::InvalidHandle, "pipeline is invalid or no render pass is active");
            glUseProgram(pipeline->program);
            if (pipeline->desc.blend_enabled)
            {
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            }
            else
            {
                glDisable(GL_BLEND);
            }
            pipeline->desc.depth_test ? glEnable(GL_DEPTH_TEST) : glDisable(GL_DEPTH_TEST);
            glDepthMask(pipeline->desc.depth_write ? GL_TRUE : GL_FALSE);
            if (pipeline->desc.cull_mode == RhiCullMode::None)
                glDisable(GL_CULL_FACE);
            else
            {
                glEnable(GL_CULL_FACE);
                glCullFace(pipeline->desc.cull_mode == RhiCullMode::Front ? GL_FRONT : GL_BACK);
            }
            glDepthFunc(pipeline->desc.depth_compare == RhiCompareFunction::LessEqual ? GL_LEQUAL :
                        pipeline->desc.depth_compare == RhiCompareFunction::Always ? GL_ALWAYS : GL_LESS);
            glBindVertexArray(m_vertex_array);
            m_pipeline = handle;
            return RhiStatus::Success();
        }

        RhiStatus OpenGLCommandList::bindTexture(TextureViewHandle handle,
                                                 SamplerHandle sampler_handle,
                                                 ShaderBindingLocation location)
        {
            OpenGLPipeline* pipeline = m_device.pipeline(m_pipeline);
            OpenGLTexture* texture = m_device.texture(handle);
            OpenGLSampler* sampler = m_device.sampler(sampler_handle);
            if (m_state != State::InRenderPass || !pipeline || !texture || !sampler)
                return RhiStatus::Failure(RhiErrorCode::InvalidHandle, "texture, sampler, or pipeline is invalid");
            const auto found = std::find_if(pipeline->desc.texture_bindings.begin(),
                                            pipeline->desc.texture_bindings.end(),
                                            [location](const TextureBindingDesc& value) { return value.location == location; });
            if (found == pipeline->desc.texture_bindings.end())
                return RhiStatus::Failure(RhiErrorCode::InvalidArgument, "texture binding is not declared by the pipeline");
            const uint32_t native_binding = static_cast<uint32_t>(
                std::distance(pipeline->desc.texture_bindings.begin(), found));
            glActiveTexture(GL_TEXTURE0 + native_binding);
            glBindTexture(texture->target, texture->id);
            glBindSampler(native_binding, sampler->id);
            return RhiStatus::Success();
        }

        RhiStatus OpenGLCommandList::bindUniformBuffer(BufferHandle handle,
                                                       ShaderBindingLocation location)
        {
            OpenGLPipeline* pipeline = m_device.pipeline(m_pipeline);
            OpenGLBuffer* buffer = m_device.buffer(handle);
            if (m_state != State::InRenderPass || !pipeline || !buffer ||
                buffer->desc.usage != RhiBufferUsage::Uniform)
                return RhiStatus::Failure(RhiErrorCode::InvalidHandle, "uniform buffer or pipeline is invalid");
            const auto found = std::find_if(pipeline->desc.uniform_buffer_bindings.begin(),
                                            pipeline->desc.uniform_buffer_bindings.end(),
                                            [location](const UniformBufferBindingDesc& value) { return value.location == location; });
            if (found == pipeline->desc.uniform_buffer_bindings.end())
                return RhiStatus::Failure(RhiErrorCode::InvalidArgument, "uniform buffer binding is not declared by the pipeline");
            const uint32_t native_binding = static_cast<uint32_t>(
                std::distance(pipeline->desc.uniform_buffer_bindings.begin(), found));
            glBindBufferBase(GL_UNIFORM_BUFFER, native_binding, buffer->id);
            return RhiStatus::Success();
        }

        RhiStatus OpenGLCommandList::bindVertexBuffer(BufferHandle handle, uint32_t slot)
        {
            OpenGLBuffer* buffer = m_device.buffer(handle);
            OpenGLPipeline* pipeline = m_device.pipeline(m_pipeline);
            if (m_state != State::InRenderPass || !buffer || buffer->desc.usage != RhiBufferUsage::Vertex || !pipeline || slot != 0)
                return RhiStatus::Failure(RhiErrorCode::InvalidHandle, "vertex buffer or pipeline is invalid");
            glBindVertexArray(m_vertex_array);
            glBindBuffer(GL_ARRAY_BUFFER, buffer->id);
            for (const VertexAttributeDesc& attribute : pipeline->desc.vertex_attributes)
            {
                glEnableVertexAttribArray(attribute.location);
                glVertexAttribPointer(attribute.location, static_cast<GLint>(attribute.component_count),
                                      GL_FLOAT, GL_FALSE, static_cast<GLsizei>(pipeline->desc.vertex_stride),
                                      reinterpret_cast<const void*>(static_cast<uintptr_t>(attribute.offset)));
            }
            return RhiStatus::Success();
        }

        RhiStatus OpenGLCommandList::bindIndexBuffer(BufferHandle handle)
        {
            OpenGLBuffer* buffer = m_device.buffer(handle);
            if (m_state != State::InRenderPass || !buffer || buffer->desc.usage != RhiBufferUsage::Index)
                return RhiStatus::Failure(RhiErrorCode::InvalidHandle, "index buffer is invalid or no render pass is active");
            glBindVertexArray(m_vertex_array);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffer->id);
            m_index_buffer = handle;
            return RhiStatus::Success();
        }

        RhiStatus OpenGLCommandList::draw(uint32_t count, uint32_t first)
        {
            OpenGLPipeline* pipeline = m_device.pipeline(m_pipeline);
            if (m_state != State::InRenderPass || !pipeline || count == 0)
                return invalidState("draw requires a pipeline and non-zero vertex count");
            const GLenum primitive = pipeline->desc.topology == RhiPrimitiveTopology::Lines ? GL_LINES : GL_TRIANGLES;
            glDrawArrays(primitive, static_cast<GLint>(first), static_cast<GLsizei>(count));
            return RhiStatus::Success();
        }

        RhiStatus OpenGLCommandList::drawIndexed(uint32_t count, uint32_t first)
        {
            OpenGLPipeline* pipeline = m_device.pipeline(m_pipeline);
            OpenGLBuffer* index = m_device.buffer(m_index_buffer);
            if (m_state != State::InRenderPass || !pipeline || !index || count == 0)
                return invalidState("drawIndexed requires pipeline, index buffer, and non-zero index count");
            const GLenum primitive = pipeline->desc.topology == RhiPrimitiveTopology::Lines ? GL_LINES : GL_TRIANGLES;
            glDrawElements(primitive, static_cast<GLsizei>(count), GL_UNSIGNED_INT,
                           reinterpret_cast<const void*>(static_cast<uintptr_t>(first * sizeof(uint32_t))));
            return RhiStatus::Success();
        }

        RhiStatus OpenGLCommandList::endRenderPass()
        {
            if (m_state != State::InRenderPass)
                return invalidState("no render pass is active");
            glBindVertexArray(0);
            glUseProgram(0);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            m_state = State::Recording;
            return RhiStatus::Success();
        }

        RhiStatus OpenGLCommandList::copyTexture(TextureViewHandle source,
                                                 TextureViewHandle destination)
        {
            if (m_state != State::Recording)
                return invalidState("copyTexture requires recording state outside a render pass");
            OpenGLTexture* src = m_device.texture(source);
            OpenGLTexture* dst = m_device.texture(destination);
            if (!src || !dst)
                return RhiStatus::Failure(RhiErrorCode::InvalidHandle, "copy texture view is stale or invalid");
            if (src->desc.width != dst->desc.width || src->desc.height != dst->desc.height ||
                src->desc.format != dst->desc.format)
                return RhiStatus::Failure(RhiErrorCode::InvalidArgument, "copy textures must have matching dimensions and format");

            const bool depth_stencil = src->desc.format == RhiFormat::Depth24Stencil8;
            const bool depth = src->desc.format == RhiFormat::Depth32Float;
            const GLenum attachment = depth_stencil ? GL_DEPTH_STENCIL_ATTACHMENT
                : (depth ? GL_DEPTH_ATTACHMENT : GL_COLOR_ATTACHMENT0);
            const GLbitfield mask = depth_stencil ? (GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT)
                : (depth ? GL_DEPTH_BUFFER_BIT : GL_COLOR_BUFFER_BIT);

            glBindFramebuffer(GL_READ_FRAMEBUFFER, m_framebuffer);
            glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
            glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, 0, 0);
            glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, 0, 0);
            glFramebufferTexture2D(GL_READ_FRAMEBUFFER, attachment, src->target, src->id, 0);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_copy_framebuffer);
            glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
            glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, 0, 0);
            glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, 0, 0);
            glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, attachment, dst->target, dst->id, 0);
            if (!depth && !depth_stencil)
            {
                glReadBuffer(GL_COLOR_ATTACHMENT0);
                glDrawBuffer(GL_COLOR_ATTACHMENT0);
            }
            else
            {
                glReadBuffer(GL_NONE);
                glDrawBuffer(GL_NONE);
            }
            if (glCheckFramebufferStatus(GL_READ_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE ||
                glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            {
                glBindFramebuffer(GL_FRAMEBUFFER, 0);
                return RhiStatus::Failure(RhiErrorCode::ResourceCreationFailed, "copy framebuffer is incomplete");
            }
            glBlitFramebuffer(0, 0, static_cast<GLint>(src->desc.width), static_cast<GLint>(src->desc.height),
                              0, 0, static_cast<GLint>(dst->desc.width), static_cast<GLint>(dst->desc.height),
                              mask, GL_NEAREST);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            return RhiStatus::Success();
        }

        RhiStatus OpenGLCommandList::readbackTexture(TextureViewHandle handle, void* destination, size_t size)
        {
            if (m_state != State::Recording)
                return invalidState("readback requires recording state outside a render pass");
            OpenGLTexture* texture = m_device.texture(handle);
            if (!texture)
                return RhiStatus::Failure(RhiErrorCode::InvalidHandle, "texture view is stale or invalid");
            GLint internal = 0;
            GLenum external = 0;
            GLenum type = 0;
            if (!textureFormat(texture->desc.format, internal, external, type))
                return RhiStatus::Failure(RhiErrorCode::InvalidArgument, "unsupported readback format");
            const size_t component_size = type == GL_UNSIGNED_BYTE ? 1u : 4u;
            const size_t component_count = external == GL_RGBA || external == GL_BGRA ? 4u : 1u;
            const size_t required = static_cast<size_t>(texture->desc.width) * texture->desc.height *
                                    component_size * component_count;
            if (!destination || size < required)
                return RhiStatus::Failure(RhiErrorCode::OutOfBounds, "readback destination is too small");

            glBindFramebuffer(GL_READ_FRAMEBUFFER, m_framebuffer);
            const bool is_depth = texture->desc.format == RhiFormat::Depth24Stencil8 ||
                                  texture->desc.format == RhiFormat::Depth32Float;
            const GLenum attachment = texture->desc.format == RhiFormat::Depth24Stencil8
                ? GL_DEPTH_STENCIL_ATTACHMENT
                : (is_depth ? GL_DEPTH_ATTACHMENT : GL_COLOR_ATTACHMENT0);
            glFramebufferTexture2D(GL_READ_FRAMEBUFFER, attachment, texture->target, texture->id, 0);
            if (attachment == GL_COLOR_ATTACHMENT0)
                glReadBuffer(GL_COLOR_ATTACHMENT0);
            glPixelStorei(GL_PACK_ALIGNMENT, 1);
            glReadPixels(0, 0, static_cast<GLsizei>(texture->desc.width),
                         static_cast<GLsizei>(texture->desc.height), external, type, destination);
            glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
            return RhiStatus::Success();
        }

        RhiStatus OpenGLCommandList::end()
        {
            if (m_state != State::Recording)
                return invalidState("command list must be recording outside a render pass");
            m_state = State::Executable;
            return RhiStatus::Success();
        }
    } // namespace

    std::unique_ptr<IRenderDevice> CreateOpenGLRenderDevice(bool validation_enabled)
    {
        if (glGetString(GL_VERSION) == nullptr)
            return nullptr;
        return std::make_unique<OpenGLRenderDevice>(validation_enabled);
    }

    RhiResult<uint64_t> ResolveOpenGLTextureNativeHandle(IRenderDevice& device,
                                                         TextureViewHandle view)
    {
        auto* gl_device = dynamic_cast<OpenGLRenderDevice*>(&device);
        if (!gl_device)
        {
            return RhiResult<uint64_t>::Failure(
                RhiErrorCode::UnsupportedBackend,
                "texture view does not belong to an OpenGL render device");
        }
        OpenGLTexture* texture = gl_device->texture(view);
        if (!texture)
        {
            return RhiResult<uint64_t>::Failure(
                RhiErrorCode::InvalidHandle,
                "texture view handle is stale or invalid");
        }
        return RhiResult<uint64_t>::Success(static_cast<uint64_t>(texture->id));
    }

    std::unique_ptr<ISwapchain> CreateOpenGLSwapchain(IWindow& window)
    {
        if (window.graphicsBackend() != GraphicsBackend::OpenGL)
            return nullptr;
        return std::make_unique<OpenGLSwapchain>(window);
    }
} // namespace Hybrid
