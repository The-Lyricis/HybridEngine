#pragma once

#include <cstdint>
#include <memory>

#include <imgui.h>


//#include "editor/panels/viewport_panel.h" 

struct GLFWwindow;

namespace Hybrid
{
    class Scene;

    // Editor context & panels（请按你的实际路径调整）
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

        void beginFrame();
        void drawPanels();                      // DockSpace + Hierarchy/Inspector/Project
        void drawViewport(uint32_t colorTexID); // 仍保留：绘制 Viewport（可在 RenderSystem 或主循环调用）
        void endFrame();

        // --- data binding ---
        void setActiveScene(Scene* scene);      // Edit 模式下的 active scene
        void setViewportTexture(uint32_t colorTexID); // 若你后续改为“UI 自己画 viewport”，可用此接口喂纹理

        EditorContext& context();               // 给外部读取 viewport 状态等（后续 picking/gizmo 会用）

    private:
        void drawDockSpaceRoot();
        void buildDefaultLayout();

        void drawMenuBar(); // 顶部菜单栏（File/Window 等）

    private:
        GLFWwindow* m_window = nullptr;
        bool m_initialized = false;

        // Docking/layout
        ImGuiID m_DockSpaceID = 0;
        bool m_DefaultLayoutBuilt = false;
        bool m_RequestResetLayout = false;

        // Shared editor state (selection, viewport rect, camera mode...)
        std::unique_ptr<EditorContext> m_ctx;

        // Panels
        std::unique_ptr<HierarchyPanel> m_HierarchyPanel;
        std::unique_ptr<InspectorPanel> m_InspectorPanel;
        std::unique_ptr<ProjectPanel>   m_ProjectPanel;
        std::unique_ptr<ViewportPanel>  m_ViewportPanel;
    };

} // namespace Hybrid

