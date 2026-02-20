#include "editor_ui.h"

#include "runtime/core/base/macro.h"

// ImGui backends
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

#include <GLFW/glfw3.h>
#include <imgui_internal.h>
#include <ImGuizmo.h>

// 请按你的实际路径调整
#include "editor/editor_context.h"
#include "editor/panels/i_editor_panel.h"
#include "editor/panels/hierarchy_panel.h"
#include "editor/panels/inspector_panel.h"
#include "editor/panels/project_panel.h"
#include "editor/panels/viewport_panel.h"

namespace Hybrid
{
    EditorUI::EditorUI() = default;

    EditorUI::~EditorUI()
    {
        // 防止用户忘记手动 shutdown 导致资源泄漏
        shutdown();
    }

    void EditorUI::initialize(GLFWwindow* window)
    {
        m_window = window;
        if (!m_window)
            return;

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable; // 启用 Docking

        ImGui::StyleColorsDark();

        // 初始化平台/渲染后端
        ImGui_ImplGlfw_InitForOpenGL(m_window, true);
        ImGui_ImplOpenGL3_Init("#version 330");

        // 创建共享上下文 & 面板
        m_ctx = std::make_unique<EditorContext>();

        m_HierarchyPanel = std::make_unique<HierarchyPanel>();
        m_InspectorPanel = std::make_unique<InspectorPanel>();
        m_ProjectPanel = std::make_unique<ProjectPanel>();
        m_ViewportPanel = std::make_unique<ViewportPanel>();

        m_initialized = true;
        HBD_CORE_TRACE("EditorUI initialized");
    }

    void EditorUI::shutdown()
    {
        if (!m_initialized)
            return;

        // 先释放面板与上下文（避免内部还引用 ImGui 资源）
        m_ViewportPanel.reset();
        m_ProjectPanel.reset();
        m_InspectorPanel.reset();
        m_HierarchyPanel.reset();
        m_ctx.reset();

        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();

        m_initialized = false;
        m_window = nullptr;

        m_DockSpaceID = 0;
        m_DefaultLayoutBuilt = false;
        m_RequestResetLayout = false;
    }

    void EditorUI::beginFrame()
    {
        if (!m_initialized)
            return;

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGuizmo::SetImGuiContext(ImGui::GetCurrentContext());
        ImGuizmo::BeginFrame();
    }

    void EditorUI::drawPanels()
    {
        if (!m_initialized || !m_ctx)
            return;

        // 1) DockSpace 宿主窗口
        drawDockSpaceRoot();

        // 2) 首帧/重置时搭建默认布局
        if (!m_DefaultLayoutBuilt || m_RequestResetLayout)
        {
            buildDefaultLayout();
            m_DefaultLayoutBuilt = true;
            m_RequestResetLayout = false;
        }

        // 3) 绘制各面板（注意：Viewport 仍可单独 drawViewport()）
        if (m_HierarchyPanel) m_HierarchyPanel->onImGuiRender(*m_ctx);
        if (m_InspectorPanel) m_InspectorPanel->onImGuiRender(*m_ctx);
        if (m_ProjectPanel)   m_ProjectPanel->onImGuiRender(*m_ctx);

        // 如果你希望“Viewport 也在这里绘制”，可取消注释：
        // if (m_ViewportPanel)  m_ViewportPanel->onImGuiRender(*m_ctx);
    }

    void EditorUI::drawViewport(uint32_t colorTexID)
    {
        if (!m_initialized || !m_ctx || !m_ViewportPanel)
            return;

        // 若 Viewport 面板被用户关闭，则不绘制
        if (!m_ViewportPanel->isOpen())
            return;

        m_ViewportPanel->setTexture(colorTexID);
        m_ViewportPanel->onImGuiRender(*m_ctx);
    }

    void EditorUI::endFrame()
    {
        if (!m_initialized)
            return;

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        // swapBuffers 建议由主循环控制
    }

    void EditorUI::setActiveScene(Scene* scene)
    {
        if (!m_ctx) return;
        m_ctx->active_scene = scene;
    }

    void EditorUI::setViewportTexture(uint32_t colorTexID)
    {
        if (!m_ViewportPanel) return;
        m_ViewportPanel->setTexture(colorTexID);
    }

    EditorContext& EditorUI::context()
    {
        // 若你担心空指针，可以在外面调用前保证 initialize 已完成
        return *m_ctx;
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
            ImGuiWindowFlags_NoDocking;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

        ImGui::Begin("##DockSpaceHost", nullptr, host_flags);

        ImGui::PopStyleVar(3);

        // DockSpace：所有子窗口都停靠在这里
        m_DockSpaceID = ImGui::GetID("HybridDockSpace");
        ImGui::DockSpace(m_DockSpaceID, ImVec2(0, 0), ImGuiDockNodeFlags_None);

        // 顶部菜单栏
        drawMenuBar();

        ImGui::End();
    }

    void EditorUI::drawMenuBar()
    {
        if (!ImGui::BeginMenuBar())
            return;

        if (ImGui::BeginMenu("File"))
        {
            ImGui::MenuItem("Exit");
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Window"))
        {
            if (m_HierarchyPanel) {
                bool open = m_HierarchyPanel->isOpen();
                if (ImGui::MenuItem("Hierarchy", nullptr, &open)) m_HierarchyPanel->setOpen(open);
            }
            if (m_InspectorPanel) {
                bool open = m_InspectorPanel->isOpen();
                if (ImGui::MenuItem("Inspector", nullptr, &open)) m_InspectorPanel->setOpen(open);
            }
            if (m_ProjectPanel) {
                bool open = m_ProjectPanel->isOpen();
                if (ImGui::MenuItem("Project", nullptr, &open)) m_ProjectPanel->setOpen(open);
            }
            if (m_ViewportPanel) {
                bool open = m_ViewportPanel->isOpen();
                if (ImGui::MenuItem("Viewport", nullptr, &open)) m_ViewportPanel->setOpen(open);
            }

            ImGui::Separator();
            if (ImGui::MenuItem("Reset Layout"))
            {
                m_RequestResetLayout = true;
                m_DefaultLayoutBuilt = false;
            }

            ImGui::EndMenu();
        }


        ImGui::EndMenuBar();
    }

    void EditorUI::buildDefaultLayout()
    {
        if (m_DockSpaceID == 0)
            return;

        ImGui::DockBuilderRemoveNode(m_DockSpaceID);
        ImGui::DockBuilderAddNode(m_DockSpaceID, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(m_DockSpaceID, ImGui::GetMainViewport()->Size);

        ImGuiID dock_main = m_DockSpaceID;
        ImGuiID dock_left = 0, dock_right = 0, dock_bottom = 0, dock_center = 0;

        ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Left, 0.20f, &dock_left, &dock_main);
        ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Right, 0.25f, &dock_right, &dock_main);
        ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Down, 0.25f, &dock_bottom, &dock_center);

        // 注意：窗口名必须与面板 getName() 返回一致
        ImGui::DockBuilderDockWindow("Hierarchy", dock_left);
        ImGui::DockBuilderDockWindow("Inspector", dock_right);
        ImGui::DockBuilderDockWindow("Project", dock_bottom);
        ImGui::DockBuilderDockWindow("Viewport", dock_center);

        ImGui::DockBuilderFinish(m_DockSpaceID);
    }

} // namespace Hybrid
