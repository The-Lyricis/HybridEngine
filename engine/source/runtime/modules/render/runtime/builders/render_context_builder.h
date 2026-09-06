#pragma once

#include "runtime/modules/render/runtime/pipeline/render_context.h"

namespace Hybrid
{
    class Framebuffer;
    class Shader;

    struct ResolvedRenderTargets
    {
        std::shared_ptr<Framebuffer> framebuffer;
        std::shared_ptr<Framebuffer> scene_framebuffer;
        std::shared_ptr<Framebuffer> selection_framebuffer;
        std::shared_ptr<Framebuffer> shadow_framebuffer;
        std::array<std::shared_ptr<Framebuffer>, kMaxDirectionalShadowCascades>* shadow_cascade_framebuffers = nullptr;
    };

    struct RenderContextBuildInput
    {
        const FrameContext* frame = nullptr;
        const RenderPacket* packet = nullptr;
        const RenderSelectionState* editor_selection = nullptr;
        RenderFlags flags = RenderFlags::None;
        void* window_handle = nullptr;
        ResolvedRenderTargets targets;
        const SelectionOverlayStyle* selection_overlay_style = nullptr;
        ShaderLibrary* shader_library = nullptr;
        std::shared_ptr<Shader> scene_shader;
        std::shared_ptr<Shader> skybox_shader;
        std::shared_ptr<Shader> shadow_shader;
        std::shared_ptr<Shader> collider_debug_shader;
    };

    class RenderContextBuilder
    {
    public:
        RenderContext build(const RenderContextBuildInput& input) const;
    };
} // namespace Hybrid
