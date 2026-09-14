#pragma once

#include <memory>

#include "runtime/modules/render/runtime/render_frame_request.h"
#include "runtime/modules/render/runtime/frame_context.h"
#include "runtime/modules/render/runtime/render_flags.h"
#include "runtime/modules/render/runtime/render_packet.h"
#include "runtime/modules/render/runtime/selection_overlay_style.h"
#include "runtime/modules/render/runtime/shader_library.h"
#include "runtime/modules/render/rhi/rhi_handles.h"

namespace Hybrid
{
    class Framebuffer;
    class IRenderDevice;
    class RenderGraphResourceRegistry;
    class Shader;

    // Internal execution context consumed by render pipeline stages.
    struct RenderContext
    {
        const FrameContext* frame = nullptr;
        const RenderPacket* packet = nullptr;
        const RenderSelectionState* editor_selection = nullptr;
        RenderFlags flags = RenderFlags::None;
        IRenderDevice* device = nullptr;
        // Shared RHI frame data. New RHI passes bind this instead of reaching
        // into the legacy OpenGL UniformBuffer implementation.
        BufferHandle frame_uniform_buffer;
        RenderGraphResourceRegistry* graph_resources = nullptr;
        std::shared_ptr<Framebuffer> framebuffer;
        std::shared_ptr<Framebuffer> scene_framebuffer;
        const SelectionOverlayStyle* selection_overlay_style = nullptr;

        ShaderLibrary* shader_library = nullptr;
        std::shared_ptr<Shader> skybox_shader;
        std::shared_ptr<Shader> collider_debug_shader;
    };
} // namespace Hybrid
