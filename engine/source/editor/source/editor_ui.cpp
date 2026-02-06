#include "editor_ui.h"
#include <imgui.h>

#include <GLFW/glfw3.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include "runtime/function/input/input_system.h"

namespace Hybrid {
    void EditorUI::initialize(GLFWwindow* window) {
        m_window = window; // 由外部传入

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        // 注意：第二个参数传 true，代表让 ImGui 自动安装它的回调
        ImGui_ImplGlfw_InitForOpenGL(m_window, true);
        ImGui_ImplOpenGL3_Init("#version 330");
    }

    void EditorUI::display() {
        if (!m_window) return;

        ImGui_ImplOpenGL3_NewFrame(); // 通知opengl开始新的一帧
        ImGui_ImplGlfw_NewFrame();    // 通知glfw开始新的一帧
        ImGui::NewFrame();            // 通知imgui开始新的一帧

        // ===== UI =====
        ImGuiIO& io = ImGui::GetIO();

        // 1) 你的主面板：只负责面板内容，不再在里面画状态栏
        ImGui::Begin("HybridEngine");
        {
            ImGui::Text("Hello, engine");
        }
        ImGui::End();

        // 2) 全局底部状态栏：固定在主视口底部（不属于 SaluteChickEngine）
        {
            ImGuiViewport* vp = ImGui::GetMainViewport();

            const float status_h = ImGui::GetFrameHeightWithSpacing();   // 状态栏高度
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

            // 可选：让状态栏不抢输入焦点
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

            ImGui::Begin("##BottomStatusBar", nullptr, flags);
            {
                // 左侧：LastKey
                const std::string lastKey = InputSystem::getInstance().getLastKeyName(); // 按你实际单例入口调整
                ImGui::Text("Last key: %s", lastKey.c_str());

                // 右侧：FPS（右对齐）
                char fpsBuf[64];
                std::snprintf(fpsBuf, sizeof(fpsBuf), "FPS: %.1f", io.Framerate);

                float fpsWidth = ImGui::CalcTextSize(fpsBuf).x;
                float rightX = ImGui::GetContentRegionAvail().x - fpsWidth;
                if (rightX > 0.0f)
                    ImGui::SameLine(rightX);

                ImGui::TextUnformatted(fpsBuf);
            }
            ImGui::End();

            ImGui::PopStyleVar(2);
        }
        // ===== Render =====
        ImGui::Render(); // 渲染数据

        int display_w = 0, display_h = 0; // 定义宽高变量
        glfwGetFramebufferSize(m_window, &display_w, &display_h); // 获取 framebuffer 的实际像素大小
        glViewport(0, 0, display_w, display_h); // 设置视口大小
        glClear(GL_COLOR_BUFFER_BIT); // 清除颜色缓冲区

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(m_window); // 交换前后缓冲区
    }
}
