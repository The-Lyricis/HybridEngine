#include "editor_ui.h"

#include <string>

#include <ImGuizmo.h>
#include <imgui_internal.h>
#include <glad/gl.h>
#include <stb_image.h>

#include "editor/core/editor_context.h"
#include "editor/tools/panels/game_view_panel.h"
#include "editor/tools/panels/hierarchy_panel.h"
#include "editor/tools/panels/inspector_panel.h"
#include "editor/tools/panels/project_panel.h"
#include "editor/tools/panels/scene_view_panel.h"
#include "runtime/core/base/macro.h"

namespace Hybrid
{
    namespace
    {
        static GLuint LoadTextureRGBA8(const std::string& path)
        {
            int w = 0, h = 0, comp = 0;
            stbi_set_flip_vertically_on_load(0);
            unsigned char* data = stbi_load(path.c_str(), &w, &h, &comp, 4);
            if (!data || w <= 0 || h <= 0)
            {
                if (data)
                    stbi_image_free(data);
                return 0;
            }

            GLuint tex = 0;
            glGenTextures(1, &tex);
            glBindTexture(GL_TEXTURE_2D, tex);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
            glBindTexture(GL_TEXTURE_2D, 0);

            stbi_image_free(data);
            return tex;
        }

        struct TopToolbarIcons
        {
            GLuint play = 0;
            GLuint pause = 0;
            GLuint stop = 0;
            bool loaded = false;

            void destroy()
            {
                auto del = [](GLuint& tex)
                {
                    if (tex)
                    {
                        glDeleteTextures(1, &tex);
                        tex = 0;
                    }
                };

                del(play);
                del(pause);
                del(stop);
                loaded = false;
            }
        };

        static TopToolbarIcons g_TopToolbarIcons;

        static void PushActiveToolStyle(bool active, bool alert = false)
        {
            if (!active)
                return;

            const ImVec4 base = alert ? ImVec4(0.80f, 0.22f, 0.22f, 0.90f) : ImVec4(0.25f, 0.45f, 0.95f, 0.90f);
            const ImVec4 hover = alert ? ImVec4(0.86f, 0.26f, 0.26f, 1.00f) : ImVec4(0.25f, 0.45f, 0.95f, 1.00f);
            const ImVec4 active_col = alert ? ImVec4(0.72f, 0.18f, 0.18f, 1.00f) : ImVec4(0.20f, 0.40f, 0.90f, 1.00f);
            ImGui::PushStyleColor(ImGuiCol_Button, base);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hover);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, active_col);
        }

        static void PopActiveToolStyle(bool active)
        {
            if (active)
                ImGui::PopStyleColor(3);
        }
    }

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
        m_SceneViewportPanel = std::make_unique<SceneViewPanel>();
        m_GameViewportPanel = std::make_unique<GameViewPanel>();

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
        g_TopToolbarIcons.destroy();

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
        drawMenuBar();
        drawTopToolbar();
        ImGui::DockSpace(m_DockSpaceID, ImVec2(0, 0), ImGuiDockNodeFlags_None);
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
            if (m_ctx && m_ctx->request_save_scene_as)
                m_ctx->request_save_scene_as();
        }
        else if (request_save_shortcut && m_ctx)
        {
            if (m_ctx->request_save_scene)
                m_ctx->request_save_scene();
        }

        if (ImGui::BeginMenu("File"))
        {
            if (!is_playing)
            {
                const bool can_save = m_ctx && static_cast<bool>(m_ctx->request_save_scene);
                if (ImGui::MenuItem("Save", "Ctrl+S", false, can_save))
                    m_ctx->request_save_scene();

                const bool can_save_as = m_ctx && static_cast<bool>(m_ctx->request_save_scene_as);
                if (ImGui::MenuItem("Save As...", "Ctrl+Shift+S", false, can_save_as))
                    m_ctx->request_save_scene_as();

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

        ImGui::EndMenuBar();
    }

    void EditorUI::drawTopToolbar()
    {
        if (!m_ctx)
            return;

        const bool is_playing = m_ctx->is_play_mode ? m_ctx->is_play_mode() : false;
        const bool is_paused = m_ctx->is_pause_mode ? m_ctx->is_pause_mode() : false;
        const float toolbar_height = 22.0f;

        if (!g_TopToolbarIcons.loaded)
        {
            const std::string base = std::string(HYBRID_ROOT_DIR) + "/resources/icons/";
            g_TopToolbarIcons.play = LoadTextureRGBA8(base + "icon_topTool_play.png");
            g_TopToolbarIcons.pause = LoadTextureRGBA8(base + "icon_topTool_pause.png");
            g_TopToolbarIcons.stop = LoadTextureRGBA8(base + "icon_topTool_stop.png");
            g_TopToolbarIcons.loaded =
                (g_TopToolbarIcons.play != 0 && g_TopToolbarIcons.pause != 0 && g_TopToolbarIcons.stop != 0);
        }

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 4.0f));
        ImGui::BeginChild("##EditorTopToolbar",
                          ImVec2(0.0f, toolbar_height),
                          ImGuiChildFlags_None,
                          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        ImGui::PopStyleVar();

        const float content_x = ImGui::GetCursorPosX();
        const float content_y = ImGui::GetCursorPosY();
        const ImVec2 content_avail = ImGui::GetContentRegionAvail();

        std::string status_line;
        if (m_ctx->active_document)
            status_line = m_ctx->active_document->isSaved() ? m_ctx->active_document->vpath : m_ctx->active_document->display_name;
        else
            status_line = "Untitled";

        if (m_ctx->active_document && m_ctx->active_document->dirty)
            status_line += " *";
        if (!m_ctx->status_message.empty())
            status_line += " | " + m_ctx->status_message;

        const ImVec2 button_size(32.0f, 20.0f);
        const ImVec2 icon_size(18.0f, 18.0f);
        const float text_y = content_y + std::max(0.0f, (content_avail.y - ImGui::GetTextLineHeight()) * 0.5f);
        const float button_y = content_y + std::max(0.0f, (content_avail.y - button_size.y) * 0.5f);

        ImGui::SetCursorPosY(text_y);
        ImGui::TextUnformatted(status_line.c_str());
        if (m_ctx->active_document && !m_ctx->active_document->native_path.empty() &&
            ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
        {
            ImGui::SetTooltip("%s", m_ctx->active_document->native_path.string().c_str());
        }

        const float gap = ImGui::GetStyle().ItemSpacing.x;
        const float controls_width = button_size.x * 2.0f + gap;
        const float start_x = content_x + std::max(0.0f, (content_avail.x - controls_width) * 0.5f);

        ImGui::SetCursorPosX(start_x);
        ImGui::SetCursorPosY(button_y);

        auto drawTopToolbarButton = [&](const char* id, GLuint icon, const char* fallback, const char* tooltip, bool active, bool alert) -> bool
        {
            PushActiveToolStyle(active, alert);
            bool pressed = false;

            if (icon != 0)
            {
                const ImVec2 cursor = ImGui::GetCursorScreenPos();
                if (ImGui::Button(id, button_size))
                    pressed = true;

                const ImVec2 icon_min(
                    cursor.x + (button_size.x - icon_size.x) * 0.5f,
                    cursor.y + (button_size.y - icon_size.y) * 0.5f);
                const ImVec2 icon_max(icon_min.x + icon_size.x, icon_min.y + icon_size.y);
                ImGui::GetWindowDrawList()->AddImage((ImTextureID)(intptr_t)icon, icon_min, icon_max);
            }
            else
            {
                pressed = ImGui::Button(fallback, button_size);
            }

            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
                ImGui::SetTooltip("%s", tooltip);

            PopActiveToolStyle(active);
            return pressed;
        };

        const bool play_active = is_playing;
        const GLuint play_icon = is_playing ? g_TopToolbarIcons.stop : g_TopToolbarIcons.play;
        if (drawTopToolbarButton("##TopToolbarPlay", play_icon, is_playing ? "Stop" : "Play", is_playing ? "Stop" : "Play", play_active, is_playing))
        {
            if (!is_playing)
            {
                if (m_ctx->enter_play_mode)
                    m_ctx->enter_play_mode();
            }
            else if (m_ctx->exit_play_mode)
            {
                m_ctx->exit_play_mode();
            }
        }

        ImGui::SameLine();
        const bool pause_enabled = is_playing && static_cast<bool>(m_ctx->toggle_pause_mode);
        if (!pause_enabled)
            ImGui::BeginDisabled();
        if (drawTopToolbarButton("##TopToolbarPause", g_TopToolbarIcons.pause, is_paused ? "Resume" : "Pause", is_paused ? "Resume" : "Pause", is_paused, false) &&
            m_ctx->toggle_pause_mode)
        {
            m_ctx->toggle_pause_mode();
        }
        if (!pause_enabled)
            ImGui::EndDisabled();

        ImGui::EndChild();
    }

    void EditorUI::buildDefaultLayout()
    {
        if (m_DockSpaceID == 0)
            return;

        ImGui::DockBuilderRemoveNode(m_DockSpaceID);
        ImGui::DockBuilderAddNode(m_DockSpaceID, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(m_DockSpaceID, ImGui::GetMainViewport()->Size);

        ImGuiID dock_main = m_DockSpaceID;
        ImGuiID dock_right = 0, dock_left_area = 0, dock_project = 0, dock_top = 0, dock_hierarchy = 0;

        ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Right, 0.25f, &dock_right, &dock_left_area);
        ImGui::DockBuilderSplitNode(dock_left_area, ImGuiDir_Down, 0.30f, &dock_project, &dock_top);
        ImGui::DockBuilderSplitNode(dock_top, ImGuiDir_Left, 0.22f, &dock_hierarchy, &dock_main);

        ImGui::DockBuilderDockWindow("Inspector", dock_right);
        ImGui::DockBuilderDockWindow("Project", dock_project);
        ImGui::DockBuilderDockWindow("Hierarchy", dock_hierarchy);
        ImGui::DockBuilderDockWindow("Scene", dock_main);
        ImGui::DockBuilderDockWindow("Game", dock_main);

        ImGui::DockBuilderFinish(m_DockSpaceID);
    }
} // namespace Hybrid
