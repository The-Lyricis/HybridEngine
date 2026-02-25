#pragma once

#include <cstdint>

namespace Hybrid
{
    // Optional editor-only extension passed to render pipeline.
    struct EditorRenderExt
    {
        bool viewport_active = true;       // Whether editor viewport currently accepts input.
        bool pan_tool = false;
        bool use_game_camera = true;       // True: use scene primary camera, false: editor camera.
        uint32_t selected_entity_id = 0;   // Selection used by outline/highlight passes.

        bool request_pick = false;         // Request one ID-buffer readback this frame.
        int pick_x = 0;                    // Pixel x in render target space.
        int pick_y = 0;                    // Pixel y in render target space.
    };
} // namespace Hybrid
