#pragma once

#include <cstdint>
#include <memory>

#include <imgui.h>

#include <functional>

struct GLFWwindow;

namespace Hybrid
{
    class Scene;
    struct EditorContext;

    class HierarchyPanel;
    class InspectorPanel;
    class ProjectPanel;
    class ViewportPanel;

    class EditorUI
    {
    public:
        EditorUI();
        ~EditorUI();

        void initialize(GLFWwindow* window);
        void shutdown();

        void drawPanels();
        void drawViewport(uint32_t colorTexID);
        void updateViewportState();

        void setActiveScene(Scene* scene);
        void setViewportTexture(uint32_t colorTexID);
        EditorContext& context();

    private:
        void drawDockSpaceRoot();
        void buildDefaultLayout();
        void drawMenuBar();

    private:
        GLFWwindow* m_window = nullptr;
        bool m_initialized = false;

        ImGuiID m_DockSpaceID = 0;
        bool m_DefaultLayoutBuilt = false;
        bool m_RequestResetLayout = false;

        std::unique_ptr<EditorContext> m_ctx;
        std::unique_ptr<HierarchyPanel> m_HierarchyPanel;
        std::unique_ptr<InspectorPanel> m_InspectorPanel;
        std::unique_ptr<ProjectPanel> m_ProjectPanel;
        std::unique_ptr<ViewportPanel> m_ViewportPanel;
    };
} // namespace Hybrid
