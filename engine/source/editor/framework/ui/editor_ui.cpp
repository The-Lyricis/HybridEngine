#include "editor_ui.h"

#include <string>

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
        m_SceneViewportPanel = std::make_unique<ViewportPanel>(ViewportPanelMode::Scene);
        m_GameViewportPanel = std::make_unique<ViewportPanel>(ViewportPanelMode::Game);

        m_initialized = true;
    }

    void EditorUI::shutdown()
    {
        if (!m_initialized)
            return;

        m_GameViewportPanel.reset();
        m_SceneViewportPanel.reset();
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

        ImGuiDockNode* dock_node = (m_DockSpaceID != 0) ? ImGui::DockBuilderGetNode(m_DockSpaceID) : nullptr;
        const bool has_saved_layout =
            dock_node &&
            (dock_node->ChildNodes[0] != nullptr ||
             dock_node->ChildNodes[1] != nullptr ||
             dock_node->Windows.Size > 0);

        if (m_RequestResetLayout || (!m_DefaultLayoutBuilt && !has_saved_layout))
        {
            buildDefaultLayout();
            m_DefaultLayoutBuilt = true;
            m_RequestResetLayout = false;
        }
        else if (!m_DefaultLayoutBuilt)
        {
            m_DefaultLayoutBuilt = true;
        }

        if (m_HierarchyPanel)
            m_HierarchyPanel->onImGuiRender(*m_ctx);
        if (m_InspectorPanel)
            m_InspectorPanel->onImGuiRender(*m_ctx);
        if (m_ProjectPanel)
            m_ProjectPanel->onImGuiRender(*m_ctx);
    }

    void EditorUI::drawViewports(uint32_t sceneColorTexID, uint32_t gameColorTexID)
    {
        if (!m_initialized || !m_ctx)
            return;

        if (m_SceneViewportPanel && m_SceneViewportPanel->isOpen())
        {
            m_SceneViewportPanel->setTexture(sceneColorTexID);
            m_SceneViewportPanel->onImGuiRender(*m_ctx);
        }

        if (m_GameViewportPanel && m_GameViewportPanel->isOpen())
        {
            m_GameViewportPanel->setTexture(gameColorTexID);
            m_GameViewportPanel->onImGuiRender(*m_ctx);
        }
    }

    void EditorUI::updateViewportState()
    {
        if (!m_initialized || !m_ctx)
            return;

        if (!m_SceneViewportPanel || !m_SceneViewportPanel->isOpen())
        {
            m_ctx->scene_viewport_hovered = false;
            m_ctx->scene_viewport_image_hovered = false;
            m_ctx->scene_viewport_focused = false;
            m_ctx->scene_viewport_size = {0.0f, 0.0f};
        }
        else
        {
            m_SceneViewportPanel->updateViewportState(*m_ctx);
        }

        if (!m_GameViewportPanel || !m_GameViewportPanel->isOpen())
        {
            m_ctx->game_viewport_hovered = false;
            m_ctx->game_viewport_image_hovered = false;
            m_ctx->game_viewport_focused = false;
            m_ctx->game_viewport_size = {0.0f, 0.0f};
        }
        else
        {
            m_GameViewportPanel->updateViewportState(*m_ctx);
        }
    }

    void EditorUI::setActiveScene(Scene* scene)
    {
        if (!m_ctx)
            return;
        m_ctx->active_scene = scene;
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

        const bool is_playing = m_ctx && m_ctx->is_play_mode ? m_ctx->is_play_mode() : false;
        const bool request_save_shortcut =
            !is_playing && m_ctx && ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false);
        const bool request_save_as_shortcut = request_save_shortcut && ImGui::GetIO().KeyShift;

        if (request_save_as_shortcut)
        {
            if (m_ctx && m_ctx->save_scene_as)
                m_ctx->save_scene_as("");
        }
        else if (request_save_shortcut && m_ctx)
        {
            if (m_ctx->current_scene_vpath.empty())
            {
                if (m_ctx->save_scene_as)
                    m_ctx->save_scene_as("");
            }
            else if (m_ctx->save_scene)
            {
                m_ctx->save_scene();
            }
        }

        if (ImGui::BeginMenu("File"))
        {
            if (!is_playing)
            {
                const bool can_save = m_ctx && static_cast<bool>(m_ctx->save_scene);
                if (ImGui::MenuItem("Save", "Ctrl+S", false, can_save))
                {
                    if (m_ctx->current_scene_vpath.empty())
                    {
                        if (m_ctx->save_scene_as)
                            m_ctx->save_scene_as("");
                    }
                    else
                        m_ctx->save_scene();
                }

                const bool can_save_as = m_ctx && static_cast<bool>(m_ctx->save_scene_as);
                if (ImGui::MenuItem("Save As...", "Ctrl+Shift+S", false, can_save_as))
                    m_ctx->save_scene_as("");

                ImGui::Separator();
            }

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
            if (m_SceneViewportPanel)
            {
                bool open = m_SceneViewportPanel->isOpen();
                if (ImGui::MenuItem("Scene", nullptr, &open))
                    m_SceneViewportPanel->setOpen(open);
            }
            if (m_GameViewportPanel)
            {
                bool open = m_GameViewportPanel->isOpen();
                if (ImGui::MenuItem("Game", nullptr, &open))
                    m_GameViewportPanel->setOpen(open);
            }

            ImGui::Separator();
            if (ImGui::MenuItem("Reset Layout"))
            {
                m_RequestResetLayout = true;
                m_DefaultLayoutBuilt = false;
            }

            ImGui::EndMenu();
        }

        ImGui::Separator();

        if (!is_playing)
        {
            if (ImGui::Button("Play"))
            {
                if (m_ctx && m_ctx->enter_play_mode)
                    m_ctx->enter_play_mode();
            }
        }
        else
        {
            if (ImGui::Button("Stop"))
            {
                if (m_ctx && m_ctx->exit_play_mode)
                    m_ctx->exit_play_mode();
            }
        }

        ImGui::SameLine();
        ImGui::TextUnformatted(is_playing ? "Mode: Play" : "Mode: Edit");

        if (m_ctx)
        {
            std::string scene_label = "Scene: ";
            scene_label += m_ctx->current_scene_vpath.empty() ? "Untitled" : m_ctx->current_scene_vpath;
            if (m_ctx->scene_dirty)
                scene_label += " *";

            ImGui::SameLine();
            ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
            ImGui::SameLine();
            ImGui::TextUnformatted(scene_label.c_str());

            if (!m_ctx->current_scene_native_path.empty() && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
                ImGui::SetTooltip("%s", m_ctx->current_scene_native_path.c_str());

            if (!m_ctx->status_message.empty())
            {
                ImGui::SameLine();
                ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
                ImGui::SameLine();
                ImGui::TextDisabled("%s", m_ctx->status_message.c_str());
            }
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
        ImGui::DockBuilderDockWindow("Scene", dock_center);
        ImGui::DockBuilderDockWindow("Game", dock_center);

        ImGui::DockBuilderFinish(m_DockSpaceID);
    }
} // namespace Hybrid


