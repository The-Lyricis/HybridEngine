#pragma once
#include <string>

struct GLFWwindow;

namespace Hybrid {

    class EditorUI {
    public:
        void initialize(GLFWwindow* window);
        void shutdown();

        // Ã¿Ö¡µ÷ÓÃ£ºBegin -> DrawPanels -> End
        void beginFrame();
        void drawPanels();
        void endFrame();

        bool isInitialized() const { return m_initialized; }

    private:
        GLFWwindow* m_window = nullptr;
        bool m_initialized = false;

        void drawBottomStatusBar();
    };

} // namespace Hybrid
