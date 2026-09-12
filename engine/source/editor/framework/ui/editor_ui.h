#pragma once

#include <array>
#include <cstdint>
#include <deque>
#include <memory>
#include <optional>

#include <imgui.h>

#include <functional>

#include "editor/core/context/editor_dialogs.h"
#include "editor/services/render/editor_image_handle.h"
#include "editor/tools/panels/i_editor_panel.h"

namespace Hybrid
{
    class Scene;
    class EditorTextureService;
    struct EditorContext;

    class HierarchyPanel;
    class InspectorPanel;
    class ProjectPanel;
    class SceneViewPanel;
    class GameViewPanel;
    class ConsolePanel;
    class ProjectSettingsPanel;
    class TasksPanel;

    class EditorUI
    {
    public:
        EditorUI();
        ~EditorUI();

        void initialize(EditorTextureService& textures);
        void shutdown();

        void drawPanels();
        void drawViewports(EditorImageHandle scene_image, EditorImageHandle game_image);
        void updateViewportState();

        void setActiveScene(Scene* scene);
        EditorContext& context();
        const EditorContext& context() const;
        void requestResetLayout();
        void queueConfirmDialog(EditorConfirmDialog dialog);

    private:
        void drawDockSpaceRoot();
        void buildDefaultLayout();
        void drawMenuBar();
        void drawTopToolbar();
        void drawConfirmDialogs();
        void registerPanels();
        const EditorPanelDescriptor* getPanelDescriptor(EditorPanelId id) const;
        IEditorPanel* getPanel(EditorPanelId id) const;
        void drawPanelToggleMenuItem(EditorPanelId id);
        const char* getPanelWindowName(EditorPanelId id) const;
        void renderViewportPanel(EditorPanelId id, EditorImageHandle image);
        void updateViewportPanelState(EditorPanelId id);

    private:
        bool m_initialized = false;
        EditorTextureService* m_textures = nullptr;

        ImGuiID m_DockSpaceID = 0;
        bool m_DefaultLayoutBuilt = false;
        bool m_RequestResetLayout = false;
        bool m_OpenConfirmDialog = false;

        std::unique_ptr<EditorContext> m_ctx;
        std::deque<EditorConfirmDialog> m_confirm_dialog_queue;
        std::optional<EditorConfirmDialog> m_active_confirm_dialog;
        std::array<IEditorPanel*, static_cast<size_t>(EditorPanelId::Count)> m_panels{};
        std::unique_ptr<HierarchyPanel> m_HierarchyPanel;
        std::unique_ptr<InspectorPanel> m_InspectorPanel;
        std::unique_ptr<ProjectPanel> m_ProjectPanel;
        std::unique_ptr<SceneViewPanel> m_SceneViewportPanel;
        std::unique_ptr<GameViewPanel> m_GameViewportPanel;
        std::unique_ptr<ConsolePanel> m_ConsolePanel;
        std::unique_ptr<ProjectSettingsPanel> m_ProjectSettingsPanel;
        std::unique_ptr<TasksPanel> m_TasksPanel;
    };
} // namespace Hybrid
