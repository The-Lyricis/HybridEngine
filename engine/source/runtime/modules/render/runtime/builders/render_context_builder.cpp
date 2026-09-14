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
        context.device = input.device;
        context.frame_uniform_buffer = input.frame_uniform_buffer;
        context.light_uniform_buffer = input.light_uniform_buffer;
        context.shadow_uniform_buffer = input.shadow_uniform_buffer;
        context.framebuffer = input.targets.framebuffer;
        context.scene_framebuffer = input.targets.scene_framebuffer;
        context.graph_resources = input.targets.graph_resources;
        context.selection_overlay_style = input.selection_overlay_style;
        context.shader_library = input.shader_library;
        context.skybox_shader = input.skybox_shader;
        context.collider_debug_shader = input.collider_debug_shader;
        return context;
    }
} // namespace Hybrid
