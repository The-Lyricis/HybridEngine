#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "runtime/core/platform/graphics_backend.h"
#include "runtime/modules/render/rhi/rhi_handles.h"

namespace Hybrid
{
    inline constexpr uint32_t kRhiResourceSetCount = 4;

    enum class RhiErrorCode
    {
        None = 0,
        UnsupportedBackend,
        InvalidArgument,
        InvalidHandle,
        OutOfBounds,
        InvalidState,
        ResourceCreationFailed,
        ShaderCompilationFailed,
        DeviceLost,
    };

    struct RhiError
    {
        RhiErrorCode code = RhiErrorCode::None;
        std::string message;
    };

    struct RhiStatus
    {
        RhiError error;
        explicit operator bool() const { return error.code == RhiErrorCode::None; }

        static RhiStatus Success() { return {}; }
        static RhiStatus Failure(RhiErrorCode code, std::string message)
        {
            return {{code, std::move(message)}};
        }
    };

    template<typename T>
    struct RhiResult
    {
        T value{};
        RhiError error;
        explicit operator bool() const { return error.code == RhiErrorCode::None; }

        static RhiResult Success(T value) { return {std::move(value), {}}; }
        static RhiResult Failure(RhiErrorCode code, std::string message)
        {
            return {{}, {code, std::move(message)}};
        }
    };

    enum class RhiBufferUsage
    {
        Vertex = 0,
        Index,
        Uniform,
        Storage,
        Staging,
    };

    enum class RhiMemoryUsage
    {
        GPUOnly = 0,
        CPUToGPU,
        GPUToCPU,
    };

    enum class RhiTextureType
    {
        Texture2D = 0,
        TextureCube,
        Texture2DArray,
    };

    enum class RhiFormat
    {
        Unknown = 0,
        R8Unorm,
        RGBA8Unorm,
        BGRA8Unorm,
        RGBA8Srgb,
        R32Uint,
        Depth24Stencil8,
        Depth32Float,
    };

    enum class RhiShaderStage
    {
        Vertex = 0,
        Fragment,
        Compute,
    };

    enum class RhiPrimitiveTopology
    {
        Triangles = 0,
        Lines,
    };

    enum class RhiCullMode
    {
        None = 0,
        Front,
        Back,
    };

    enum class RhiCompareFunction
    {
        Always = 0,
        Less,
        LessEqual,
    };

    struct RenderDeviceDesc
    {
        GraphicsBackend backend = GraphicsBackend::OpenGL;
        bool validation_enabled = false;
    };

    struct RenderCapabilities
    {
        uint32_t max_texture_dimension_2d = 1;
        uint32_t max_color_attachments = 1;
        uint32_t uniform_buffer_alignment = 1;
        bool supports_compute = false;
        bool supports_texture_arrays = false;
    };

    struct BufferDesc
    {
        size_t size = 0;
        RhiBufferUsage usage = RhiBufferUsage::Vertex;
        RhiMemoryUsage memory = RhiMemoryUsage::GPUOnly;
        std::string debug_name;
    };

    struct RhiTextureDesc
    {
        RhiTextureType type = RhiTextureType::Texture2D;
        RhiFormat format = RhiFormat::RGBA8Unorm;
        uint32_t width = 1;
        uint32_t height = 1;
        uint32_t layers = 1;
        uint32_t mip_levels = 1;
        bool render_target = false;
        bool shader_read = true;
        std::string debug_name;
    };

    struct SamplerDesc
    {
        bool linear_filter = true;
        bool clamp_to_edge = false;
        std::string debug_name;
    };

    struct ShaderModuleDesc
    {
        RhiShaderStage stage = RhiShaderStage::Vertex;
        std::vector<uint8_t> code;
        std::string entry_point = "main";
        std::string debug_name;
    };

    struct VertexAttributeDesc
    {
        uint32_t location = 0;
        uint32_t offset = 0;
        uint32_t component_count = 0;
    };

    struct ShaderBindingLocation
    {
        uint32_t set = 0;
        uint32_t binding = 0;

        bool operator==(const ShaderBindingLocation& other) const
        {
            return set == other.set && binding == other.binding;
        }
    };

    // Temporary explicit binding metadata. M2 replaces these names with Slang
    // reflection while command recording keeps the same set/binding contract.
    struct TextureBindingDesc
    {
        ShaderBindingLocation location;
        std::string name;
    };

    struct UniformBufferBindingDesc
    {
        ShaderBindingLocation location;
        std::string name;
    };

    struct GraphicsPipelineDesc
    {
        ShaderHandle vertex_shader;
        ShaderHandle fragment_shader;
        std::vector<VertexAttributeDesc> vertex_attributes;
        std::vector<TextureBindingDesc> texture_bindings;
        std::vector<UniformBufferBindingDesc> uniform_buffer_bindings;
        uint32_t vertex_stride = 0;
        RhiPrimitiveTopology topology = RhiPrimitiveTopology::Triangles;
        RhiCullMode cull_mode = RhiCullMode::Back;
        RhiCompareFunction depth_compare = RhiCompareFunction::Less;
        bool depth_test = true;
        bool depth_write = true;
        bool blend_enabled = false;
        RhiFormat color_format = RhiFormat::RGBA8Unorm;
        RhiFormat depth_format = RhiFormat::Depth32Float;
        std::string debug_name;
    };

    struct ColorAttachmentDesc
    {
        TextureViewHandle texture;
        float clear_color[4] = {0.0f, 0.0f, 0.0f, 1.0f};
        bool clear = true;
        bool store = true;
    };

    struct DepthAttachmentDesc
    {
        TextureViewHandle texture;
        float clear_depth = 1.0f;
        bool clear = true;
        bool store = true;
    };

    struct RenderPassDesc
    {
        std::vector<ColorAttachmentDesc> colors;
        DepthAttachmentDesc depth;
        bool has_depth = false;
        std::string debug_name;
    };

    struct ViewportDesc
    {
        float x = 0.0f;
        float y = 0.0f;
        float width = 1.0f;
        float height = 1.0f;
        float min_depth = 0.0f;
        float max_depth = 1.0f;
    };
} // namespace Hybrid
