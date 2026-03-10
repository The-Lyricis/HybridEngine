#pragma once

#include <functional>

#include <entt/entt.hpp>
#include <glm/mat4x4.hpp>
#include <imgui.h>

#include "editor/core/editor_types.h"

namespace Hybrid
{
    class Scene;

    struct EditorContext
    {
        Scene* active_scene = nullptr;

        entt::entity selected = entt::null;

        bool viewport_hovered = false;
        bool viewport_focused = false;
        ImVec2 viewport_size = {1.0f, 1.0f};
        ImVec2 viewport_min = {0.0f, 0.0f};
        ImVec2 viewport_max = {0.0f, 0.0f};

        bool request_pick = false;
        int pick_x = 0;
        int pick_y = 0;

        glm::mat4 gizmo_view = glm::mat4(1.0f);
        glm::mat4 gizmo_proj = glm::mat4(1.0f);

        bool gizmo_using = false;
        bool use_game_camera = false;
        bool viewport_image_hovered = false;

        std::function<void(const AssetSourceEvent&)> notify_asset_source_event;
        std::function<void(const std::string& scene_vpath)> open_scene;
        bool pan_tool = false;

        std::function<void()> enter_play_mode;
        std::function<void()> exit_play_mode;
        std::function<bool()> is_play_mode;

        bool show_collider_debug = true;
    };
} // namespace Hybrid
