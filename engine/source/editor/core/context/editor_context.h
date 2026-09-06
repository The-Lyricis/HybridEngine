#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <entt/entt.hpp>
#include <glm/mat4x4.hpp>
#include <imgui.h>

#include "editor/core/commands/editor_commands.h"
#include "editor/core/context/editor_dialogs.h"
#include "editor/core/context/document_service.h"
#include "editor/core/context/selection_service.h"
#include "editor/core/editor_types.h"
#include "editor/core/snapshot/transform_snapshot.h"
#include "editor/services/import/import_types.h"
#include "runtime/modules/asset/asset_type.h"
#include "runtime/modules/asset/builtin_assets.h"

namespace Hybrid
{
    class Scene;
    class IEditorCommand;
    struct RenderStats;

    enum class GizmoSpace
    {
        Local = 0,
        World
    };

    enum class SceneToolMode
    {
        Select = 0,
        Move,
        Rotate,
        Scale
    };

    enum class SceneEntityTemplate : int
    {
        Empty = 0,
        Cube,
        Camera,
        DirectionalLight,
        PointLight
    };

    struct EditorViewportState
    {
        bool hovered = false;
        bool focused = false;
        ImVec2 size = {1.0f, 1.0f};
        ImVec2 min = {0.0f, 0.0f};
        ImVec2 max = {0.0f, 0.0f};
        ImVec2 render_size = {1.0f, 1.0f};
        bool image_hovered = false;
    };

    enum class GameViewResolutionMode : uint8_t
    {
        Free = 0,
        Aspect16x9,
        HD720,
        FullHD,
        Custom,
    };

    struct GameViewSettings
    {
        GameViewResolutionMode mode = GameViewResolutionMode::Free;
        int custom_width = 1280;
        int custom_height = 720;
    };

    struct EditorPickingState
    {
        bool request = false;
        int x = 0;
        int y = 0;
        bool toggle = false;
    };

    struct EditorGizmoState
    {
        glm::mat4 view = glm::mat4(1.0f);
        glm::mat4 proj = glm::mat4(1.0f);
        GizmoSpace space = GizmoSpace::Local;
        SceneToolMode tool_mode = SceneToolMode::Move;
        bool using_gizmo = false;
        bool select_tool = false;
        bool suppress_tool_shortcuts = false;
    };

    struct EditorDebugState
    {
        const RenderStats* render_stats = nullptr;
        bool show_collider_debug = true;
        bool show_shadow_debug = false;
        bool enable_post_process = false;
        bool enable_tone_mapping = false;
        bool enable_gamma_correction = false;
        float post_process_exposure = 1.0f;
        float post_process_gamma = 2.2f;
    };

    struct EditorDocumentActions
    {
        std::function<void(const std::string& scene_vpath)> open_scene;
        std::function<bool()> request_open_project;
        std::function<bool(const std::filesystem::path&)> request_open_recent_project;
        std::function<std::vector<std::filesystem::path>()> list_recent_projects;
        std::function<bool()> request_open_scene;
        std::function<bool()> request_new_scene;
        std::function<void()> request_reset_layout;
        std::function<bool()> request_save_scene;
        std::function<bool()> request_save_scene_as;
        std::function<void()> request_exit;
        std::function<void(EditorConfirmDialog)> request_confirm_dialog;
        std::function<bool(const std::filesystem::path&)> reveal_in_file_browser;
    };

    struct EditorSceneActions
    {
        std::function<entt::entity(SceneEntityTemplate, entt::entity)> create_entity;
        std::function<bool(entt::entity)> delete_entity;
        std::function<bool(entt::entity)> duplicate_selection;
        std::function<bool(AssetID, const ImVec2& drop_mouse_pos)> instantiate_asset;
        std::function<bool(const std::string& rel_path, const ImVec2& drop_mouse_pos)> instantiate_project_path;
        std::function<bool(entt::entity)> fit_box_collider_to_mesh;
        std::function<AssetID(BuiltinMesh)> get_builtin_mesh_id;
    };

    struct EditorAssetActions
    {
        std::function<void(const AssetSourceEvent&)> notify_asset_source_event;
        std::function<bool(const std::string& asset_vpath)> request_reimport_asset;
        std::function<bool(const std::string& old_folder_vpath, const std::string& new_folder_vpath)> request_rename_folder;
        std::function<AssetID(const std::string& asset_vpath)> find_asset_by_vpath;
        std::function<std::string(entt::entity)> describe_mesh_renderer_material;
        std::function<std::string(AssetID)> describe_asset;
        std::function<std::vector<ImportTaskSnapshot>()> list_import_tasks;
        std::function<bool(uint64_t)> retry_import_task;
        std::function<bool(const std::string&)> reveal_asset_source;
    };

    struct EditorCommandActions
    {
        std::function<void(std::unique_ptr<IEditorCommand>)> submit_editor_command;
        std::function<bool()> undo;
        std::function<bool()> redo;
        std::function<bool()> can_undo;
        std::function<bool()> can_redo;
        std::function<bool(EditorCommandId)> execute_command;
        std::function<bool(EditorCommandId)> can_execute_command;
        std::function<void(entt::entity, const TransformSnapshot&, const TransformSnapshot&)> commit_transform_command;
    };

    struct EditorModeActions
    {
        std::function<void()> enter_play_mode;
        std::function<void()> exit_play_mode;
        std::function<void()> toggle_pause_mode;
        std::function<bool()> is_play_mode;
        std::function<bool()> is_pause_mode;
    };

    struct EditorProjectActions
    {
        std::function<std::vector<AssetMetadata>()> list_scene_assets;
        std::function<bool(const std::string&, std::string&)> set_startup_scene;
    };

    struct EditorContext
    {
        DocumentService document;

        SelectionService selection;
        std::string status_message;

        EditorViewportState scene_viewport;
        EditorViewportState game_viewport;
        GameViewSettings game_view_settings;
        EditorPickingState picking;
        EditorGizmoState gizmo;
        EditorDebugState debug;
        EditorDocumentActions documents;
        EditorSceneActions scene_actions;
        EditorAssetActions asset_actions;
        EditorCommandActions commands;
        EditorModeActions mode;
        EditorProjectActions project;

        void setActiveDocument(std::shared_ptr<SceneDocument> document)
        {
            this->document.setActiveDocument(std::move(document));
        }

        void clearActiveDocument()
        {
            document.clear();
        }

        entt::entity activeEntity() const
        {
            return selection.active();
        }

        Scene* activeScene() const
        {
            return document.activeScene();
        }

        const std::shared_ptr<SceneDocument>& activeDocument() const
        {
            return document.activeDocument();
        }

        void markSceneDirty()
        {
            if (mode.is_play_mode && mode.is_play_mode())
                return;
            document.markDirty();
        }

        void setStatusMessage(std::string message)
        {
            status_message = std::move(message);
        }
    };
} // namespace Hybrid
