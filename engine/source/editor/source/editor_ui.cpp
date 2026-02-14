#include "editor_ui.h"

#include <GLFW/glfw3.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <imgui_internal.h>


namespace Hybrid {

    void EditorUI::initialize(GLFWwindow* window) {
        m_window = window;
        if (!m_window) return;

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable; //启用docking

        // 在这里设置 ImGui 风格
        ImGui::StyleColorsDark();

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

    void EditorUI::drawPanels() {
        if (!m_initialized) return;

        drawDockSpaceRoot();

        if (!m_DefaultLayoutBuilt || m_RequestResetLayout)
        {
            buildDefaultLayout();
            m_DefaultLayoutBuilt = true;
            m_RequestResetLayout = false;
        }

        ImGui::Begin("Hierarchy");  ImGui::Text("Hierarchy");  ImGui::End();
        ImGui::Begin("Inspector");  ImGui::Text("Inspector");  ImGui::End();
        ImGui::Begin("Project");    ImGui::Text("Project");    ImGui::End();

        // 关键：Viewport 必须真的被 Begin 出来，DockBuilder 才能把它 dock 到中心
        //drawViewport(0); // 在renderSystem中调用了
    }

    void EditorUI::drawDockSpaceRoot()
    {
        ImGuiViewport* vp = ImGui::GetMainViewport();

        ImGui::SetNextWindowPos(vp->Pos);
        ImGui::SetNextWindowSize(vp->Size);
        ImGui::SetNextWindowViewport(vp->ID);

        const ImGuiWindowFlags host_flags =
            ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoBringToFrontOnFocus |
            ImGuiWindowFlags_NoNavFocus |
            ImGuiWindowFlags_MenuBar |
            ImGuiWindowFlags_NoDocking; // 宿主本身不允许被 dock

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

        ImGui::Begin("##DockSpaceHost", nullptr, host_flags);

        ImGui::PopStyleVar(3);

        // DockSpace：所有子窗口（Hierarchy/Inspector/Viewport/Project）都停靠在这里
        m_DockSpaceID = ImGui::GetID("HybridDockSpace");
        ImGui::DockSpace(m_DockSpaceID, ImVec2(0, 0), ImGuiDockNodeFlags_None);

        // 顶部菜单栏（先留空或简单写点）
        if (ImGui::BeginMenuBar())
        {
            if (ImGui::BeginMenu("File"))
            {
                ImGui::MenuItem("Exit");
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Window"))
            {
                if (ImGui::MenuItem("Reset Layout"))
                {
                    m_RequestResetLayout = true;
                    m_DefaultLayoutBuilt = false;
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }

        ImGui::End();
    }

    void EditorUI::buildDefaultLayout()
    {
        if (m_DockSpaceID == 0) return; // Host 未创建则不建

        ImGui::DockBuilderRemoveNode(m_DockSpaceID);
        ImGui::DockBuilderAddNode(m_DockSpaceID, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(m_DockSpaceID, ImGui::GetMainViewport()->Size);

        ImGuiID dock_main = m_DockSpaceID;
        ImGuiID dock_left = 0, dock_right = 0, dock_bottom = 0, dock_center = 0;

        ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Left, 0.20f, &dock_left, &dock_main);
        ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Right, 0.25f, &dock_right, &dock_main);
        ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Down, 0.25f, &dock_bottom, &dock_center);

        ImGui::DockBuilderDockWindow("Hierarchy", dock_left);
        ImGui::DockBuilderDockWindow("Inspector", dock_right);
        ImGui::DockBuilderDockWindow("Project", dock_bottom);
        ImGui::DockBuilderDockWindow("Viewport", dock_center);

        ImGui::DockBuilderFinish(m_DockSpaceID);
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

