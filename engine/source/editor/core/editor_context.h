#pragma once

#include <functional>
#include <memory>
#include <string>

#include <entt/entt.hpp>
#include <glm/mat4x4.hpp>
#include <imgui.h>

#include "editor/core/editor_types.h"
#include "editor/core/scene_document.h"
#include "runtime/modules/asset/asset_type.h"
#include "runtime/modules/asset/builtin_assets.h"

namespace Hybrid
{
    class Scene;

    struct EditorContext
    {
        Scene* active_scene = nullptr;
        std::shared_ptr<SceneDocument> active_document;

        entt::entity selected = entt::null;
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
        std::function<bool()> request_save_scene;
        std::function<bool()> request_save_scene_as;
        std::function<AssetID(BuiltinMesh)> get_builtin_mesh_id;
        bool pan_tool = false;
        bool suppress_tool_shortcuts = false;

        std::function<void()> enter_play_mode;
        std::function<void()> exit_play_mode;
        std::function<void()> toggle_pause_mode;
        std::function<bool()> is_play_mode;

        bool show_collider_debug = true;
        std::function<bool()> is_pause_mode;

        void setActiveDocument(std::shared_ptr<SceneDocument> document)
        {
            active_document = std::move(document);
            active_scene = active_document ? active_document->scene.get() : nullptr;
        }

        void clearActiveDocument()
        {
            active_document.reset();
            active_scene = nullptr;
        }

        void markSceneDirty()
        {
            if (active_document)
                active_document->dirty = true;
        }

        void setStatusMessage(std::string message)
        {
            status_message = std::move(message);
        }
    };
} // namespace Hybrid
