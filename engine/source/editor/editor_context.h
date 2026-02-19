#pragma once
#include <imgui.h>
#include <entt/entt.hpp>

namespace Hybrid
{
    class Scene;

    struct EditorContext
    {
        Scene* active_scene = nullptr;

        entt::entity selected = entt::null;

        bool viewport_hovered = false;
        bool viewport_focused = false;

        ImVec2 viewport_size = ImVec2(1, 1);
        ImVec2 viewport_min = ImVec2(0, 0);
        ImVec2 viewport_max = ImVec2(0, 0);

        bool use_game_camera = false;
        bool request_pick = false;
        int  pick_x = 0;
        int  pick_y = 0;
    };
}
