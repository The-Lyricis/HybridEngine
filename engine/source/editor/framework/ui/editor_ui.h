#pragma once

#include <array>
#include <cstdint>
#include <deque>
#include <memory>
#include <optional>

#include <imgui.h>

#include <functional>

#include "editor/core/context/editor_dialogs.h"
#include "editor/tools/panels/i_editor_panel.h"

struct GLFWwindow;

namespace Hybrid
{
    class Scene;
    struct EditorContext;

    class HierarchyPanel;
    class InspectorPanel;
    class ProjectPanel;
    class SceneViewPanel;
    class GameViewPanel;

    class EditorUI
    {
    public:
        EditorUI();
        ~EditorUI();

        void initialize(GLFWwindow* window);
        void shutdown();

        void drawPanels();
        void drawViewports(uint32_t sceneColorTexID, uint32_t gameColorTexID);
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
        void renderViewportPanel(EditorPanelId id, uint32_t colorTexID);
        void updateViewportPanelState(EditorPanelId id);

    private:
        GLFWwindow* m_window = nullptr;
        bool m_initialized = false;

        ImGuiID m_DockSpaceID = 0;
        bool m_DefaultLayoutBuilt = false;
        bool m_RequestResetLayout = false;
        bool m_OpenConfirmDialog = false;

        std::unique_ptr<EditorContext> m_ctx;
        std::deque<EditorConfirmDialog> m_confirm_dialog_queue;
        std::optional<EditorConfirmDialog> m_active_confirm_dialog;
        std::array<IEditorPanel*, 5> m_panels{};
        std::unique_ptr<HierarchyPanel> m_HierarchyPanel;
        std::unique_ptr<InspectorPanel> m_InspectorPanel;
        std::unique_ptr<ProjectPanel> m_ProjectPanel;
        std::unique_ptr<SceneViewPanel> m_SceneViewportPanel;
        std::unique_ptr<GameViewPanel> m_GameViewportPanel;
    };
} // namespace Hybrid
