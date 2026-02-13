#include "editor_ui.h"

#include <imgui.h>
#include <GLFW/glfw3.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>


namespace Hybrid {

    void EditorUI::initialize(GLFWwindow* window) {
        m_window = window;
        if (!m_window) return;

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        // 在这里设置 ImGui 风格
        // ImGui::StyleColorsDark();

        // 让 ImGui 安装 GLFW 回调（原本就是 true）
        ImGui_ImplGlfw_InitForOpenGL(m_window, true);

        // 3.3 Core 
        ImGui_ImplOpenGL3_Init("#version 330");

        m_initialized = true;
    }

    void EditorUI::shutdown() {
        if (!m_initialized) return;

        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();

        m_initialized = false;
        m_window = nullptr;
    }

    void EditorUI::beginFrame() {
        if (!m_initialized) return;

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
    }

    /*void EditorUI::drawBottomStatusBar() {
        ImGuiIO& io = ImGui::GetIO();
        ImGuiViewport* vp = ImGui::GetMainViewport();

        const float status_h = ImGui::GetFrameHeightWithSpacing();
        const ImVec2 pos(vp->Pos.x, vp->Pos.y + vp->Size.y - status_h);
        const ImVec2 size(vp->Size.x, status_h);

        ImGui::SetNextWindowPos(pos);
        ImGui::SetNextWindowSize(size);

        const ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoBringToFrontOnFocus |
            ImGuiWindowFlags_NoFocusOnAppearing |
            ImGuiWindowFlags_NoNav |
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoScrollWithMouse;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

        ImGui::Begin("##BottomStatusBar", nullptr, flags);
        {
            const std::string lastKey = InputSystem::getInstance().getLastKeyName();
            ImGui::Text("Last key: %s", lastKey.c_str());

            char fpsBuf[64];
            std::snprintf(fpsBuf, sizeof(fpsBuf), "FPS: %.1f", io.Framerate);

            const float fpsWidth = ImGui::CalcTextSize(fpsBuf).x;
            const float rightX = ImGui::GetContentRegionAvail().x - fpsWidth;
            if (rightX > 0.0f)
                ImGui::SameLine(rightX);

            ImGui::TextUnformatted(fpsBuf);
        }
        ImGui::End();

        ImGui::PopStyleVar(2);
    }*/

    void EditorUI::drawPanels() {
        if (!m_initialized) return;

        ImGui::Begin("HybridEngine");
        {
            ImGui::Text("Hello, engine");
            ImGui::Text("UI is rendering on top of the scene.");
        }
        ImGui::End();

        //drawBottomStatusBar();
    }

    void EditorUI::endFrame() {
        if (!m_initialized) return;

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        // 注意：swapBuffers 不应该在这里做（由 main 控制更清晰）
    }

    void Hybrid::EditorUI::drawViewport(uint32_t colorTextureID)
    {
        ImGui::Begin("Viewport");

        // --- Toolbar: Camera Mode Toggle ---
        {
            // 显示当前模式
            ImGui::TextUnformatted("Camera:");
            ImGui::SameLine();

            const char* modeText = m_UseGameCamera ? "Game" : "Editor";
            ImGui::Text("%s", modeText);
            ImGui::SameLine();

            // 切换按钮
            if (ImGui::Button(m_UseGameCamera ? "Switch to Editor Camera" : "Switch to Game Camera"))
            {
                m_UseGameCamera = !m_UseGameCamera;
            }

            // 分隔线，让 toolbar 和 viewport 内容视觉隔开
            ImGui::Separator();
        }

        m_ViewportFocused = ImGui::IsWindowFocused();
        m_ViewportHovered = ImGui::IsWindowHovered();

        ImVec2 avail = ImGui::GetContentRegionAvail();
        if (avail.x < 1.0f) avail.x = 1.0f;
        if (avail.y < 1.0f) avail.y = 1.0f;
        m_ViewportSize = { avail.x, avail.y };

        ImGui::Image(
            (ImTextureID)(intptr_t)colorTextureID,
            avail,
            ImVec2(0, 1),
            ImVec2(1, 0)
        );

        ImGui::End();
    }
}

