#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <entt/entt.hpp>
#include <glm/mat4x4.hpp>
#include <imgui.h>

#include "editor/core/editor_commands.h"
#include "editor/core/editor_dialogs.h"
#include "editor/core/editor_types.h"
#include "editor/core/scene_document.h"
#include "editor/core/transform_snapshot.h"
#include "runtime/modules/asset/asset_type.h"
#include "runtime/modules/asset/builtin_assets.h"

namespace Hybrid
{
    class Scene;
    class IEditorCommand;
    struct RenderStats;

    struct EditorSelection
    {
        std::vector<entt::entity> items;
        entt::entity active = entt::null;
        entt::entity range_anchor = entt::null;

        void clear()
        {
            items.clear();
            active = entt::null;
            range_anchor = entt::null;
        }

        bool empty() const
        {
            return items.empty();
        }

        size_t size() const
        {
            return items.size();
        }

        bool contains(entt::entity entity) const
        {
            for (entt::entity item : items)
            {
                if (item == entity)
                    return true;
            }
            return false;
        }

        void setSingle(entt::entity entity)
        {
            if (entity == entt::null)
            {
                clear();
                return;
            }

            items.clear();
            items.push_back(entity);
            active = entity;
            range_anchor = entity;
        }

        void add(entt::entity entity)
        {
            if (entity == entt::null || contains(entity))
                return;
            items.push_back(entity);
            active = entity;
            range_anchor = entity;
        }

        void remove(entt::entity entity)
        {
            if (entity == entt::null)
                return;

            for (auto it = items.begin(); it != items.end(); ++it)
            {
                if (*it != entity)
                    continue;

                items.erase(it);
                if (active == entity)
                    active = entt::null;
                if (range_anchor == entity)
                    range_anchor = entt::null;
                return;
            }
        }

        void toggle(entt::entity entity)
        {
            if (entity == entt::null)
                return;

            if (contains(entity))
            {
                remove(entity);
                return;
            }

            add(entity);
        }
    };

    enum class GizmoSpace
    {
        Local = 0,
        World
    };

    enum class SceneEntityTemplate : int
    {
        Empty = 0,
        Cube,
        Camera,
        DirectionalLight,
        PointLight
    };

    struct EditorContext
    {
        Scene* active_scene = nullptr;
        std::shared_ptr<SceneDocument> active_document;

        EditorSelection selection;
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
        const RenderStats* render_stats = nullptr;

        bool request_pick = false;
        int pick_x = 0;
        int pick_y = 0;
        bool pick_toggle = false;

        glm::mat4 gizmo_view = glm::mat4(1.0f);
        glm::mat4 gizmo_proj = glm::mat4(1.0f);
        GizmoSpace gizmo_space = GizmoSpace::Local;

        bool gizmo_using = false;

        std::function<void(const AssetSourceEvent&)> notify_asset_source_event;
        std::function<void(const std::string& scene_vpath)> open_scene;
        std::function<bool()> request_open_project;
        std::function<bool(const std::filesystem::path&)> request_open_recent_project;
        std::function<std::vector<std::filesystem::path>()> list_recent_projects;
        std::function<bool()> request_open_scene;
        std::function<bool(const std::string& asset_vpath)> request_reimport_asset;
        std::function<bool(const std::string& old_folder_vpath, const std::string& new_folder_vpath)> request_rename_folder;
        std::function<bool()> request_new_scene;
        std::function<void()> request_reset_layout;
        std::function<bool()> request_save_scene;
        std::function<bool()> request_save_scene_as;
        std::function<entt::entity(SceneEntityTemplate, entt::entity)> create_scene_entity;
        std::function<bool(entt::entity)> delete_scene_entity;
        std::function<bool(entt::entity)> duplicate_scene_selection;
        std::function<void(std::unique_ptr<IEditorCommand>)> submit_editor_command;
        std::function<bool()> undo;
        std::function<bool()> redo;
        std::function<bool()> can_undo;
        std::function<bool()> can_redo;
        std::function<bool(EditorCommandId)> execute_command;
        std::function<bool(EditorCommandId)> can_execute_command;
        std::function<void(entt::entity, const TransformSnapshot&, const TransformSnapshot&)> commit_transform_command;
        std::function<void(EditorConfirmDialog)> request_confirm_dialog;
        std::function<bool(const std::filesystem::path&)> reveal_in_file_browser;
        std::function<AssetID(const std::string& asset_vpath)> find_asset_by_vpath;
        std::function<std::string(entt::entity)> describe_mesh_renderer_material;
        std::function<bool(AssetID, const ImVec2& drop_mouse_pos)> instantiate_scene_asset;
        std::function<bool(const std::string& rel_path, const ImVec2& drop_mouse_pos)> instantiate_scene_project_path;
        std::function<bool(entt::entity)> fit_box_collider_to_mesh;
        std::function<AssetID(BuiltinMesh)> get_builtin_mesh_id;
        bool select_tool = false;
        bool suppress_tool_shortcuts = false;

        std::function<void()> enter_play_mode;
        std::function<void()> exit_play_mode;
        std::function<void()> toggle_pause_mode;
        std::function<bool()> is_play_mode;

        bool show_collider_debug = true;
        bool show_shadow_debug = false;
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

        entt::entity activeEntity() const
        {
            return selection.active;
        }

        void markSceneDirty()
        {
            if (is_play_mode && is_play_mode())
                return;
            if (active_document)
                active_document->dirty = true;
        }

        void setStatusMessage(std::string message)
        {
            status_message = std::move(message);
        }
    };
} // namespace Hybrid
