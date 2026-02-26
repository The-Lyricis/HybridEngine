#include "editor_ui.h"

#include <ImGuizmo.h>
#include <imgui_internal.h>

#include "editor/core/editor_context.h"
#include "editor/tools/panels/hierarchy_panel.h"
#include "editor/tools/panels/inspector_panel.h"
#include "editor/tools/panels/project_panel.h"
#include "editor/tools/panels/viewport_panel.h"
#include "runtime/core/base/macro.h"

namespace Hybrid
{
    EditorUI::EditorUI() = default;

    EditorUI::~EditorUI()
    {
        shutdown();
    }

    void EditorUI::initialize(GLFWwindow* window)
    {
        m_window = window;
        if (!m_window)
            return;

        if (ImGui::GetCurrentContext() == nullptr)
        {
            HBD_CORE_ERROR("EditorUI initialize failed: ImGui context is null");
            return;
        }

        m_ctx = std::make_unique<EditorContext>();
        m_HierarchyPanel = std::make_unique<HierarchyPanel>();
        m_InspectorPanel = std::make_unique<InspectorPanel>();
        m_ProjectPanel = std::make_unique<ProjectPanel>();
        m_ViewportPanel = std::make_unique<ViewportPanel>();

        m_initialized = true;
    }

    void EditorUI::shutdown()
    {
        if (!m_initialized)
            return;

        m_ViewportPanel.reset();
        m_ProjectPanel.reset();
        m_InspectorPanel.reset();
        m_HierarchyPanel.reset();
        m_ctx.reset();

        m_initialized = false;
        m_window = nullptr;
        m_DockSpaceID = 0;
        m_DefaultLayoutBuilt = false;
        m_RequestResetLayout = false;
    }

    void EditorUI::drawPanels()
    {
        if (!m_initialized || !m_ctx)
            return;

        ImGuizmo::SetImGuiContext(ImGui::GetCurrentContext());
        ImGuizmo::BeginFrame();

        drawDockSpaceRoot();

        if (!m_DefaultLayoutBuilt || m_RequestResetLayout)
        {
            buildDefaultLayout();
            m_DefaultLayoutBuilt = true;
            m_RequestResetLayout = false;
        }

        if (m_HierarchyPanel)
            m_HierarchyPanel->onImGuiRender(*m_ctx);
        if (m_InspectorPanel)
            m_InspectorPanel->onImGuiRender(*m_ctx);
        if (m_ProjectPanel)
            m_ProjectPanel->onImGuiRender(*m_ctx);
    }

    void EditorUI::drawViewport(uint32_t colorTexID)
    {
        if (!m_initialized || !m_ctx || !m_ViewportPanel)
            return;

        if (!m_ViewportPanel->isOpen())
            return;

        m_ViewportPanel->setTexture(colorTexID);
        m_ViewportPanel->onImGuiRender(*m_ctx);
    }

    void EditorUI::setActiveScene(Scene* scene)
    {
        if (!m_ctx)
            return;
        m_ctx->active_scene = scene;
    }

    void EditorUI::setViewportTexture(uint32_t colorTexID)
    {
        if (!m_ViewportPanel)
            return;
        m_ViewportPanel->setTexture(colorTexID);
    }

    EditorContext& EditorUI::context()
    {
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

        m_DockSpaceID = ImGui::GetID("HybridDockSpace");
        ImGui::DockSpace(m_DockSpaceID, ImVec2(0, 0), ImGuiDockNodeFlags_None);
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
            if (m_HierarchyPanel)
            {
                bool open = m_HierarchyPanel->isOpen();
                if (ImGui::MenuItem("Hierarchy", nullptr, &open))
                    m_HierarchyPanel->setOpen(open);
            }
            if (m_InspectorPanel)
            {
                bool open = m_InspectorPanel->isOpen();
                if (ImGui::MenuItem("Inspector", nullptr, &open))
                    m_InspectorPanel->setOpen(open);
            }
            if (m_ProjectPanel)
            {
                bool open = m_ProjectPanel->isOpen();
                if (ImGui::MenuItem("Project", nullptr, &open))
                    m_ProjectPanel->setOpen(open);
            }
            if (m_ViewportPanel)
            {
                bool open = m_ViewportPanel->isOpen();
                if (ImGui::MenuItem("Viewport", nullptr, &open))
                    m_ViewportPanel->setOpen(open);
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

        ImGui::DockBuilderDockWindow("Hierarchy", dock_left);
        ImGui::DockBuilderDockWindow("Inspector", dock_right);
        ImGui::DockBuilderDockWindow("Project", dock_bottom);
        ImGui::DockBuilderDockWindow("Viewport", dock_center);

        ImGui::DockBuilderFinish(m_DockSpaceID);
    }
} // namespace Hybrid


