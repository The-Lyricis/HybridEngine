#include "render_context_builder.h"

namespace Hybrid
{
    RenderContext RenderContextBuilder::build(const RenderContextBuildInput& input) const
    {
        RenderContext context{};
        context.frame = input.frame;
        context.packet = input.packet;
        context.editor_selection = input.editor_selection;
        context.flags = input.flags;
        context.window_handle = input.window_handle;
        context.framebuffer = input.targets.framebuffer;
        context.scene_framebuffer = input.targets.scene_framebuffer;
        context.selection_framebuffer = input.targets.selection_framebuffer;
        context.shadow_framebuffer = input.targets.shadow_framebuffer;
        context.shadow_cascade_framebuffers = input.targets.shadow_cascade_framebuffers;
        context.selection_overlay_style = input.selection_overlay_style;
        context.shader_library = input.shader_library;
        context.scene_shader = input.scene_shader;
        context.skybox_shader = input.skybox_shader;
        context.shadow_shader = input.shadow_shader;
        context.collider_debug_shader = input.collider_debug_shader;
        return context;
    }
} // namespace Hybrid
