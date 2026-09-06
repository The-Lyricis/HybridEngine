#pragma once

#include <filesystem>
#include <functional>
#include <memory>
#include <string>

#include <entt/entity/fwd.hpp>

#include "editor/core/commands/editor_commands.h"
#include "editor/core/commands/editor_command_history.h"
#include "editor/framework/camera/editor_camera.h"
#include "editor/framework/bindings/editor_context_action_bindings.h"
#include "editor/framework/controllers/editor_asset_hot_reload_controller.h"
#include "editor/framework/ui/editor_ui.h"
#include "editor/core/engine_services.h"
#include "editor/services/scene/editor_scene_io_service.h"
#include "runtime/core/event/layer.h"

namespace Hybrid
{
    enum class SceneEntityTemplate : int;

    struct EditorModeCallbacks
    {
        std::function<bool(std::shared_ptr<Scene>)> enter_play_mode_from_scene;
        std::function<void()> exit_play_mode;
        std::function<void()> toggle_pause_mode;
        std::function<bool()> is_play_mode;
        std::function<bool()> is_pause_mode;
    };
    // Editor orchestration layer: UI draw + bridge from EditorContext to render inputs.
    class EditorLayer final : public Layer
    {
    public:
        explicit EditorLayer(EngineServices services);

        void onAttach() override;          // Initialize editor UI and bind scene.
        void onDetach() override;          // Release editor UI resources.
        void onUpdate(float dt) override;  // Sync viewport state, camera input and render ext.
        void onImGuiRender() override;     // Draw panels and viewport.

        void setModeCallbacks(EditorModeCallbacks callbacks);
        void requestExit();

    private:
        void syncContextDocumentState();
        void syncSceneViewState();
        void updateFrameContext();         // Push current editor state into FrameContext/Flags/Ext.
        void updateEditorCamera(float dt);
        EditorCommandContext makeCommandContext();
        bool executeCommand(EditorCommandId id);
        bool canExecuteCommand(EditorCommandId id) const;
        bool requestOpenProject();
        bool openProjectInNewInstance(const std::filesystem::path& requested_project_path);
        bool requestDocumentTransition(std::string action, std::function<bool()> transition);
        EditorDocumentActions buildDocumentActions();
        EditorSceneActions buildSceneActions();
        EditorAssetActions buildAssetActions();
        EditorCommandActions buildCommandActions();
        EditorModeActions buildModeActions();
        EditorProjectActions buildProjectActions();
        EditorContextActionBindings buildContextActionBindings();
        AssetID findAssetByVPath(const std::string& asset_vpath) const;
        std::string describeMeshRendererMaterial(entt::entity entity_handle) const;
        bool instantiateSceneAsset(AssetID asset_id, const ImVec2& drop_mouse_pos);
        bool instantiateSceneProjectPath(const std::string& rel_path, const ImVec2& drop_mouse_pos);
        bool tryGetSceneDropPosition(const ImVec2& drop_mouse_pos, glm::vec3& out_position);
        bool fitBoxColliderToMesh(entt::entity entity_handle);
        entt::entity createSceneEntity(SceneEntityTemplate type, entt::entity parent);
        bool deleteSceneEntity(entt::entity entity_handle);
        bool duplicateSceneSelection(entt::entity target_entity);

    private:
        EngineServices m_services{};       // Injected runtime/editor services.
        EditorUI m_editor_ui;              // Panel/UI owner.
        EditorCamera m_editor_camera;      // Editor-only viewport camera.
        EditorAssetHotReloadController m_asset_hot_reload_controller;
        EditorSceneIOService m_scene_io;
        EditorCommandDispatcher m_command_dispatcher;
        CommandHistory m_command_history;
        std::shared_ptr<SceneDocument> m_active_scene_view_document;
        bool m_initialized = false;        // Guard against partial startup/shutdown.
        bool m_document_transition_pending = false;
        EditorModeCallbacks m_mode_callbacks{};
    };
} // namespace Hybrid

