#include <cstdint>
#include <array>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "runtime/core/platform/window.h"
#include "runtime/modules/render/public/graphics_context.h"
#include "runtime/modules/render/public/renderer_api.h"
#include "runtime/modules/render/rhi/render_device.h"

namespace
{
    int failures = 0;

    void expect(bool condition, const char* expression, int line)
    {
        if (!condition)
        {
            std::cerr << "FAILED line " << line << ": " << expression << '\n';
            ++failures;
        }
    }

#define EXPECT(expression) expect(static_cast<bool>(expression), #expression, __LINE__)

    std::vector<uint8_t> shaderCode(const char* source)
    {
        const char* end = source;
        while (*end != '\0')
            ++end;
        return {source, end};
    }
}

int main()
{
    std::cerr << "stage: create-window\n";
    EXPECT(Hybrid::RendererAPI::setAPI(Hybrid::GraphicsBackend::OpenGL));
    auto window = Hybrid::CreatePlatformWindow();
    EXPECT(window != nullptr);
    if (!window)
        return 1;

    Hybrid::WindowDesc window_desc{};
    window_desc.width = 64;
    window_desc.height = 64;
    window_desc.title = "Hybrid OpenGL RHI Contract Tests";
    window_desc.visible = false;
    window_desc.graphics_backend = Hybrid::GraphicsBackend::OpenGL;
    std::string window_error;
    EXPECT(window->initialize(window_desc, window_error));
    if (!window_error.empty())
        std::cerr << window_error << '\n';
#if defined(__APPLE__)
    const Hybrid::NativeWindowHandle native_handle = window->nativeHandle();
    EXPECT(native_handle.kind == Hybrid::NativeWindowKind::Cocoa);
    EXPECT(native_handle.window != nullptr);
    EXPECT(native_handle.view != nullptr);
#endif

    std::cerr << "stage: create-context\n";
    auto context = Hybrid::GraphicsContext::Create(*window);
    EXPECT(context != nullptr);
    if (!context)
        return 1;
    context->init();

    std::cerr << "stage: create-device\n";
    auto device = Hybrid::CreateOpenGLRenderDevice();
    EXPECT(device != nullptr);
    if (!device)
        return 1;
    EXPECT(device->backend() == Hybrid::GraphicsBackend::OpenGL);
    EXPECT(device->capabilities().max_color_attachments >= 1);

    Hybrid::RhiTextureDesc texture_desc{};
    texture_desc.width = 32;
    texture_desc.height = 32;
    texture_desc.format = Hybrid::RhiFormat::RGBA8Unorm;
    texture_desc.render_target = true;
    const auto texture = device->createTexture(texture_desc);
    EXPECT(texture);
    const auto view = device->createTextureView(texture.value);
    EXPECT(view);
    const auto copy_texture = device->createTexture(texture_desc);
    EXPECT(copy_texture);
    const auto copy_view = device->createTextureView(copy_texture.value);
    EXPECT(copy_view);

    Hybrid::RhiTextureDesc sampled_desc{};
    sampled_desc.width = 2;
    sampled_desc.height = 2;
    sampled_desc.format = Hybrid::RhiFormat::RGBA8Unorm;
    const std::array<uint8_t, 16> sampled_pixels = {
        255, 0, 0, 255, 255, 0, 0, 255,
        255, 0, 0, 255, 255, 0, 0, 255,
    };
    const auto sampled_texture = device->createTexture(
        sampled_desc, sampled_pixels.data(), sampled_pixels.size());
    EXPECT(sampled_texture);
    const auto sampled_view = device->createTextureView(sampled_texture.value);
    EXPECT(sampled_view);
    Hybrid::SamplerDesc sampler_desc{};
    sampler_desc.linear_filter = true;
    sampler_desc.clamp_to_edge = true;
    const auto sampler = device->createSampler(sampler_desc);
    EXPECT(sampler);

    const std::array<float, 4> tint = {1.0f, 1.0f, 1.0f, 1.0f};
    Hybrid::BufferDesc uniform_desc{};
    uniform_desc.size = sizeof(tint);
    uniform_desc.usage = Hybrid::RhiBufferUsage::Uniform;
    uniform_desc.memory = Hybrid::RhiMemoryUsage::CPUToGPU;
    const auto uniform_buffer = device->createBuffer(uniform_desc, tint.data(), sizeof(tint));
    EXPECT(uniform_buffer);

    static constexpr const char* vertex_source = R"GLSL(#version 330 core
layout(location = 0) in vec2 position;
void main() { gl_Position = vec4(position, 0.0, 1.0); }
)GLSL";
    static constexpr const char* fragment_source = R"GLSL(#version 330 core
layout(location = 0) out vec4 color;
uniform sampler2D sourceTexture;
layout(std140) uniform TestSettings { vec4 tint; };
void main() { color = texture(sourceTexture, vec2(0.5)) * tint; }
)GLSL";

    Hybrid::ShaderModuleDesc vertex_shader_desc{};
    vertex_shader_desc.stage = Hybrid::RhiShaderStage::Vertex;
    vertex_shader_desc.code = shaderCode(vertex_source);
    const auto vertex_shader = device->createShaderModule(vertex_shader_desc);
    EXPECT(vertex_shader);
    Hybrid::ShaderModuleDesc fragment_shader_desc{};
    fragment_shader_desc.stage = Hybrid::RhiShaderStage::Fragment;
    fragment_shader_desc.code = shaderCode(fragment_source);
    const auto fragment_shader = device->createShaderModule(fragment_shader_desc);
    EXPECT(fragment_shader);

    Hybrid::GraphicsPipelineDesc pipeline_desc{};
    pipeline_desc.vertex_shader = vertex_shader.value;
    pipeline_desc.fragment_shader = fragment_shader.value;
    pipeline_desc.vertex_stride = sizeof(float) * 2;
    pipeline_desc.vertex_attributes.push_back({0, 0, 2});
    pipeline_desc.texture_bindings.push_back({{2, 0}, "sourceTexture"});
    pipeline_desc.uniform_buffer_bindings.push_back({{3, 0}, "TestSettings"});
    pipeline_desc.cull_mode = Hybrid::RhiCullMode::None;
    pipeline_desc.depth_test = false;
    pipeline_desc.depth_write = false;
    const auto pipeline = device->createGraphicsPipeline(pipeline_desc);
    EXPECT(pipeline);

    const float triangle[] = {
         0.0f,  0.8f,
        -0.8f, -0.8f,
         0.8f, -0.8f,
    };
    Hybrid::BufferDesc vertex_buffer_desc{};
    vertex_buffer_desc.size = sizeof(triangle);
    vertex_buffer_desc.usage = Hybrid::RhiBufferUsage::Vertex;
    const auto vertex_buffer = device->createBuffer(vertex_buffer_desc, triangle, sizeof(triangle));
    EXPECT(vertex_buffer);

    Hybrid::RenderPassDesc pass{};
    Hybrid::ColorAttachmentDesc color_attachment{};
    color_attachment.texture = view.value;
    color_attachment.clear_color[0] = 0.0f;
    color_attachment.clear_color[1] = 0.0f;
    color_attachment.clear_color[2] = 1.0f;
    color_attachment.clear_color[3] = 1.0f;
    pass.colors.push_back(color_attachment);

    std::cerr << "stage: draw\n";
    auto commands = device->createCommandList();
    EXPECT(commands->begin());
    EXPECT(commands->beginRenderPass(pass));
    EXPECT(commands->setViewport({0.0f, 0.0f, 32.0f, 32.0f}));
    EXPECT(commands->bindPipeline(pipeline.value));
    EXPECT(commands->bindTexture(sampled_view.value, sampler.value, {2, 0}));
    EXPECT(commands->bindUniformBuffer(uniform_buffer.value, {3, 0}));
    EXPECT(commands->bindVertexBuffer(vertex_buffer.value));
    EXPECT(commands->draw(3));
    EXPECT(commands->endRenderPass());
    EXPECT(commands->copyTexture(view.value, copy_view.value));

    std::vector<uint8_t> pixels(32u * 32u * 4u);
    std::cerr << "stage: readback\n";
    EXPECT(commands->readbackTexture(copy_view.value, pixels.data(), pixels.size()));
    EXPECT(commands->end());
    EXPECT(device->submit(*commands));
    const size_t center = (16u * 32u + 16u) * 4u;
    EXPECT(pixels[center] >= 240u);
    EXPECT(pixels[center + 1] <= 16u);
    EXPECT(pixels[center + 2] <= 16u);

    std::cerr << "stage: cleanup\n";
    EXPECT(device->destroyBuffer(uniform_buffer.value));
    EXPECT(device->destroyBuffer(vertex_buffer.value));
    EXPECT(device->destroySampler(sampler.value));
    EXPECT(device->destroyPipeline(pipeline.value));
    EXPECT(device->destroyShaderModule(vertex_shader.value));
    EXPECT(device->destroyShaderModule(fragment_shader.value));
    EXPECT(device->destroyTextureView(sampled_view.value));
    EXPECT(device->destroyTexture(sampled_texture.value));
    EXPECT(device->destroyTextureView(copy_view.value));
    EXPECT(device->destroyTexture(copy_texture.value));
    EXPECT(device->destroyTextureView(view.value));
    EXPECT(device->destroyTexture(texture.value));
    commands.reset();
    device.reset();
    context.reset();
    window->shutdown();
    std::cerr << "stage: complete\n";

    if (failures != 0)
    {
        std::cerr << failures << " OpenGL RHI contract assertion(s) failed\n";
        return 1;
    }
    std::cout << "HybridOpenGLRhiContractTests: all checks passed\n";
    return 0;
}
