#pragma once
#include <string>
#include <glm/vec2.hpp>
#include <imgui.h>

struct GLFWwindow;

namespace Hybrid {

    class EditorUI {
    public:
        void initialize(GLFWwindow* window);
        void shutdown();

        // 每帧调用：Begin -> DrawPanels -> End
        void beginFrame();
        void drawPanels();
        void endFrame();

        bool isInitialized() const { return m_initialized; }

        void drawViewport(uint32_t colorTextureID);

        glm::vec2 getViewportSize() const { return m_ViewportSize; }
        bool isViewportFocused() const { return m_ViewportFocused; }
        bool isViewportHovered() const { return m_ViewportHovered; }

        bool useGameCamera() const { return m_UseGameCamera; }

        void buildDefaultLayout();

    private:
        GLFWwindow* m_window = nullptr;
        bool m_initialized = false;

        void drawBottomStatusBar();
        void drawDockSpaceRoot();

        glm::vec2 m_ViewportSize{ 0.0f, 0.0f };
        bool m_ViewportFocused = false;
        bool m_ViewportHovered = false;

        bool m_UseGameCamera = false;

        bool m_DefaultLayoutBuilt = false;
        bool m_RequestResetLayout = true;

        ImGuiID m_DockSpaceID = 0;
    };

} // namespace Hybrid
