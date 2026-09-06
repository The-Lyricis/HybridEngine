#pragma once

#include <array>
#include <memory>

#include "runtime/modules/render/runtime/render_frame_request.h"
#include "runtime/modules/render/runtime/frame_context.h"
#include "runtime/modules/render/runtime/render_flags.h"
#include "runtime/modules/render/runtime/render_packet.h"
#include "runtime/modules/render/runtime/render_shadow_settings.h"
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
        const RenderSelectionState* editor_selection = nullptr;
        RenderFlags flags = RenderFlags::None;
        void* window_handle = nullptr;
        std::shared_ptr<Framebuffer> framebuffer;
        std::shared_ptr<Framebuffer> scene_framebuffer;
        std::shared_ptr<Framebuffer> selection_framebuffer;
        std::shared_ptr<Framebuffer> shadow_framebuffer;
        std::array<std::shared_ptr<Framebuffer>, kMaxDirectionalShadowCascades>* shadow_cascade_framebuffers = nullptr;
        const SelectionOverlayStyle* selection_overlay_style = nullptr;

        ShaderLibrary* shader_library = nullptr;
        std::shared_ptr<Shader> scene_shader;
        std::shared_ptr<Shader> skybox_shader;
        std::shared_ptr<Shader> shadow_shader;
        std::shared_ptr<Shader> collider_debug_shader;
    };
} // namespace Hybrid
