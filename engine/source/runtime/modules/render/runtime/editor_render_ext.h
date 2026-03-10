#pragma once

#include <cstdint>

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace Hybrid
{
    // Optional editor-only extension passed to render pipeline.
    struct EditorRenderExt
    {
        bool viewport_active = true;       // Whether editor viewport currently accepts input.
        bool render_scene_view = false;
        bool render_game_view = false;
        glm::vec2 scene_viewport_size = glm::vec2(0.0f);
        glm::vec2 game_viewport_size = glm::vec2(0.0f);
        bool pan_tool = false;
        bool use_game_camera = true;       // True: use scene primary camera, false: editor camera.
        uint32_t selected_entity_id = 0;   // Selection used by outline/highlight passes.

        bool request_pick = false;         // Request one ID-buffer readback this frame.
        bool show_collider_debug = false;
        int pick_x = 0;                    // Pixel x in render target space.
        int pick_y = 0;                    // Pixel y in render target space.

        // Editor camera payload supplied by editor module when use_game_camera is false.
        bool has_editor_camera = false;
        glm::mat4 editor_view = glm::mat4(1.0f);
        glm::mat4 editor_proj = glm::mat4(1.0f);
        glm::vec3 editor_camera_pos = glm::vec3(0.0f, 0.0f, 3.0f);
    };
} // namespace Hybrid
