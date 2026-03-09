#pragma once

#include <functional>
#include <string>

#include <entt/entt.hpp>
#include <glm/mat4x4.hpp>
#include <imgui.h>

#include "editor/core/editor_types.h"
#include "runtime/modules/scene/scene.h"

namespace Hybrid
{
    struct EditorContext
    {
        Scene* active_scene = nullptr;

        entt::entity selected = entt::null;
        std::string current_scene_vpath;
        std::string current_scene_native_path;
        bool scene_dirty = false;
        std::string status_message;

        bool scene_viewport_hovered = false;
        bool scene_viewport_focused = false;
        ImVec2 scene_viewport_size = {1.0f, 1.0f};
        ImVec2 scene_viewport_min = {0.0f, 0.0f};
        ImVec2 scene_viewport_max = {0.0f, 0.0f};
        bool scene_viewport_image_hovered = false;

        bool game_viewport_hovered = false;
        bool game_viewport_focused = false;
        ImVec2 game_viewport_size = {1.0f, 1.0f};
        ImVec2 game_viewport_min = {0.0f, 0.0f};
        ImVec2 game_viewport_max = {0.0f, 0.0f};
        bool game_viewport_image_hovered = false;

        bool request_pick = false;
        int pick_x = 0;
        int pick_y = 0;

        glm::mat4 gizmo_view = glm::mat4(1.0f);
        glm::mat4 gizmo_proj = glm::mat4(1.0f);

        bool gizmo_using = false;

        std::function<void(const AssetSourceEvent&)> notify_asset_source_event;
        std::function<void(const std::string& scene_vpath)> open_scene;
        std::function<bool()> save_scene;
        std::function<bool(const std::string& scene_vpath)> save_scene_as;
        bool pan_tool = false;
        bool suppress_tool_shortcuts = false;

        std::function<void()> enter_play_mode;
        std::function<void()> exit_play_mode;
        std::function<bool()> is_play_mode;

        void setSceneDocument(std::string scene_vpath, std::string scene_native_path)
        {
            current_scene_vpath = std::move(scene_vpath);
            current_scene_native_path = std::move(scene_native_path);
            scene_dirty = false;
            if (active_scene)
            {
                active_scene->setCurrentSceneVPath(current_scene_vpath);
                active_scene->setDirty(false);
            }
        }

        void clearSceneDocument()
        {
            current_scene_vpath.clear();
            current_scene_native_path.clear();
            scene_dirty = false;
            if (active_scene)
            {
                active_scene->setCurrentSceneVPath("");
                active_scene->setDirty(false);
            }
        }

        void markSceneDirty()
        {
            scene_dirty = true;
            if (active_scene)
                active_scene->setDirty(true);
        }

        void setStatusMessage(std::string message)
        {
            status_message = std::move(message);
        }
    };
} // namespace Hybrid
