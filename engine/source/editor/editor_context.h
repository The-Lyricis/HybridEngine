#pragma once

#include <cstdint>
#include <functional>
#include <string>

#include <entt/entt.hpp>
#include <glm/mat4x4.hpp>
#include <imgui.h>

namespace Hybrid
{
    class Scene;

    enum class AssetSourceEventType : uint8_t
    {
        Added = 0,
        Modified,
        Removed,
        Moved
    };

    struct AssetSourceEvent
    {
        AssetSourceEventType type = AssetSourceEventType::Modified;
        std::string path;      // Added/Modified/Removed 用
        std::string old_path;  // Moved 用
        std::string new_path;  // Moved 用
    };

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

        // Notify editor asset pipeline about source file events.
        std::function<void(const AssetSourceEvent&)> notify_asset_source_event;
        bool pan_tool = false;
    };
} // namespace Hybrid
