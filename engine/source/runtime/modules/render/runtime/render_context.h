#pragma once

#include <memory>

#include "runtime/modules/render/runtime/editor_render_ext.h"
#include "runtime/modules/render/runtime/frame_context.h"
#include "runtime/modules/render/runtime/render_flags.h"
#include "runtime/modules/render/runtime/render_packet.h"
#include "runtime/modules/render/runtime/selection_overlay_style.h"
#include "runtime/modules/render/runtime/shader_library.h"

namespace Hybrid
{
    class Framebuffer;
    class Shader;

    // Internal execution context consumed by render pipeline stages.
    struct RenderContext
    {
        const FrameContext* frame = nullptr;
        const RenderPacket* packet = nullptr;
        const EditorSelectionState* editor_selection = nullptr;
        RenderFlags flags = RenderFlags::None;
        void* window_handle = nullptr;
        std::shared_ptr<Framebuffer> framebuffer;
        std::shared_ptr<Framebuffer> scene_framebuffer;
        std::shared_ptr<Framebuffer> selection_framebuffer;
        const SelectionOverlayStyle* selection_overlay_style = nullptr;

        ShaderLibrary* shader_library = nullptr;
        std::shared_ptr<Shader> scene_shader;
        std::shared_ptr<Shader> collider_debug_shader;
    };
} // namespace Hybrid
