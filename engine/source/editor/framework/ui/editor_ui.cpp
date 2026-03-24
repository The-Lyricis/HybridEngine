#include "editor_ui.h"

#include <string>

#include <ImGuizmo.h>
#include <imgui_internal.h>
#include <glad/gl.h>
#include <stb_image.h>

#include "editor/core/editor_commands.h"
#include "editor/core/editor_dialogs.h"
#include "editor/core/editor_context.h"
#include "editor/core/editor_shortcuts.h"
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
        constexpr const char* kEditorUILogTag = "[EditorUI]";
        static bool g_TopToolbarIconLoadFailedLogged = false;
        enum class EditorLayoutNode
        {
            Main,
            Right,
            LeftArea,
            LeftBottom,
            LeftTop,
            LeftTopLeft,
            Count,
        };

        enum class EditorLayoutSplitDirection
        {
            Left,
            Right,
            Up,
            Down,
        };

        struct EditorLayoutSplitDesc
        {
            EditorLayoutNode source = EditorLayoutNode::Main;
            EditorLayoutSplitDirection direction = EditorLayoutSplitDirection::Right;
            float ratio = 0.5f;
            EditorLayoutNode out_primary = EditorLayoutNode::Main;
            EditorLayoutNode out_secondary = EditorLayoutNode::Main;
        };

        constexpr EditorPanelDescriptor kPanelDescriptors[] = {
            {EditorPanelId::Hierarchy, "Hierarchy", true, true, false, EditorDockSlot::LeftTopLeft},
            {EditorPanelId::Inspector, "Inspector", true, true, false, EditorDockSlot::Right},
            {EditorPanelId::Project, "Project", true, true, false, EditorDockSlot::LeftBottom},
            {EditorPanelId::SceneView, "Scene", true, true, true, EditorDockSlot::Main},
            {EditorPanelId::GameView, "Game", true, true, true, EditorDockSlot::Main},
        };
        constexpr EditorLayoutSplitDesc kDefaultLayoutSplits[] = {
            {EditorLayoutNode::Main, EditorLayoutSplitDirection::Right, 0.25f, EditorLayoutNode::Right, EditorLayoutNode::LeftArea},
            {EditorLayoutNode::LeftArea, EditorLayoutSplitDirection::Down, 0.30f, EditorLayoutNode::LeftBottom, EditorLayoutNode::LeftTop},
            {EditorLayoutNode::LeftTop, EditorLayoutSplitDirection::Left, 0.22f, EditorLayoutNode::LeftTopLeft, EditorLayoutNode::Main},
        };
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

        ImGuiDir toImGuiDir(EditorLayoutSplitDirection direction)
        {
            switch (direction)
            {
            case EditorLayoutSplitDirection::Left:
                return ImGuiDir_Left;
            case EditorLayoutSplitDirection::Right:
                return ImGuiDir_Right;
            case EditorLayoutSplitDirection::Up:
                return ImGuiDir_Up;
            case EditorLayoutSplitDirection::Down:
                return ImGuiDir_Down;
            }

            return ImGuiDir_Right;
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
            HBD_CORE_ERROR("{} initialize_failed reason=imgui_context_is_null", kEditorUILogTag);
            return;
        }

        m_ctx = std::make_unique<EditorContext>();
        m_HierarchyPanel = std::make_unique<HierarchyPanel>();
        m_InspectorPanel = std::make_unique<InspectorPanel>();
        m_ProjectPanel = std::make_unique<ProjectPanel>();
        m_SceneViewportPanel = std::make_unique<SceneViewPanel>();
        m_GameViewportPanel = std::make_unique<GameViewPanel>();
        registerPanels();

        m_initialized = true;
        HBD_CORE_INFO("{} initialize_completed", kEditorUILogTag);
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
        m_panels.fill(nullptr);
        m_ctx.reset();
        g_TopToolbarIcons.destroy();

        m_initialized = false;
        m_window = nullptr;
        m_DockSpaceID = 0;
        m_DefaultLayoutBuilt = false;
        m_RequestResetLayout = false;
        m_OpenConfirmDialog = false;
        m_active_confirm_dialog.reset();
        m_confirm_dialog_queue.clear();
        HBD_CORE_INFO("{} shutdown_completed", kEditorUILogTag);
    }

    void EditorUI::drawPanels()
    {
        if (!m_initialized || !m_ctx)
            return;

        ProcessEditorShortcuts(*m_ctx);

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

        for (const EditorPanelDescriptor& descriptor : kPanelDescriptors)
        {
            if (descriptor.is_viewport)
                continue;

            if (IEditorPanel* panel = getPanel(descriptor.id))
                panel->onImGuiRender(*m_ctx);
        }

        drawConfirmDialogs();
    }

    void EditorUI::drawViewports(uint32_t sceneColorTexID, uint32_t gameColorTexID)
    {
        if (!m_initialized || !m_ctx)
            return;

        for (const EditorPanelDescriptor& descriptor : kPanelDescriptors)
        {
            if (!descriptor.is_viewport)
                continue;

            switch (descriptor.id)
            {
            case EditorPanelId::SceneView:
                renderViewportPanel(descriptor.id, sceneColorTexID);
                break;
            case EditorPanelId::GameView:
                renderViewportPanel(descriptor.id, gameColorTexID);
                break;
            default:
                break;
            }
        }
    }

    void EditorUI::updateViewportState()
    {
        if (!m_initialized || !m_ctx)
            return;

        for (const EditorPanelDescriptor& descriptor : kPanelDescriptors)
        {
            if (descriptor.is_viewport)
                updateViewportPanelState(descriptor.id);
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

    const EditorContext& EditorUI::context() const
    {
        return *m_ctx;
    }

    void EditorUI::requestResetLayout()
    {
        m_RequestResetLayout = true;
        m_DefaultLayoutBuilt = false;
    }

    void EditorUI::queueConfirmDialog(EditorConfirmDialog dialog)
    {
        m_confirm_dialog_queue.push_back(std::move(dialog));
    }

    void EditorUI::registerPanels()
    {
        m_panels.fill(nullptr);
        m_panels[static_cast<size_t>(EditorPanelId::Hierarchy)] = m_HierarchyPanel.get();
        m_panels[static_cast<size_t>(EditorPanelId::Inspector)] = m_InspectorPanel.get();
        m_panels[static_cast<size_t>(EditorPanelId::Project)] = m_ProjectPanel.get();
        m_panels[static_cast<size_t>(EditorPanelId::SceneView)] = m_SceneViewportPanel.get();
        m_panels[static_cast<size_t>(EditorPanelId::GameView)] = m_GameViewportPanel.get();

        for (const EditorPanelDescriptor& descriptor : kPanelDescriptors)
        {
            if (IEditorPanel* panel = getPanel(descriptor.id))
                panel->setOpen(descriptor.default_open);
        }
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

        if (ImGui::BeginMenu("File"))
        {
            const bool can_open_project = m_ctx && m_ctx->can_execute_command &&
                m_ctx->can_execute_command(EditorCommandId::OpenProject);
            if (ImGui::MenuItem("Open Project...", "Ctrl+Shift+O", false, can_open_project))
                m_ctx->execute_command(EditorCommandId::OpenProject);

            if (ImGui::BeginMenu("Open Recent Project"))
            {
                bool has_recent_projects = false;
                if (m_ctx && m_ctx->list_recent_projects && m_ctx->request_open_recent_project)
                {
                    const std::vector<std::filesystem::path> recent_projects = m_ctx->list_recent_projects();
                    for (const auto& project_path : recent_projects)
                    {
                        if (project_path.empty())
                            continue;

                        has_recent_projects = true;
                        std::string label = project_path.stem().string();
                        if (label.empty())
                            label = project_path.filename().string();
                        if (label.empty())
                            label = project_path.generic_string();
                        if (ImGui::MenuItem(label.c_str()))
                            m_ctx->request_open_recent_project(project_path);

                        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
                            ImGui::SetTooltip("%s", project_path.generic_string().c_str());
                    }
                }

                if (!has_recent_projects)
                {
                    ImGui::BeginDisabled();
                    ImGui::MenuItem("No Recent Projects");
                    ImGui::EndDisabled();
                }

                ImGui::EndMenu();
            }

            ImGui::Separator();

            if (!is_playing)
            {
                const bool can_new_scene = m_ctx && m_ctx->can_execute_command &&
                    m_ctx->can_execute_command(EditorCommandId::NewScene);
                if (ImGui::MenuItem("New Scene", "Ctrl+N", false, can_new_scene))
                    m_ctx->execute_command(EditorCommandId::NewScene);

                const bool can_open_scene = m_ctx && m_ctx->can_execute_command &&
                    m_ctx->can_execute_command(EditorCommandId::OpenScene);
                if (ImGui::MenuItem("Open Scene...", "Ctrl+O", false, can_open_scene))
                    m_ctx->execute_command(EditorCommandId::OpenScene);

                ImGui::Separator();

                const bool can_save = m_ctx && m_ctx->can_execute_command &&
                    m_ctx->can_execute_command(EditorCommandId::SaveScene);
                if (ImGui::MenuItem("Save", "Ctrl+S", false, can_save))
                    m_ctx->execute_command(EditorCommandId::SaveScene);

                const bool can_save_as = m_ctx && m_ctx->can_execute_command &&
                    m_ctx->can_execute_command(EditorCommandId::SaveSceneAs);
                if (ImGui::MenuItem("Save As...", "Ctrl+Shift+S", false, can_save_as))
                    m_ctx->execute_command(EditorCommandId::SaveSceneAs);

                ImGui::Separator();
            }

            ImGui::MenuItem("Exit");
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Window"))
        {
            for (const EditorPanelDescriptor& descriptor : kPanelDescriptors)
            {
                if (descriptor.show_in_window_menu)
                    drawPanelToggleMenuItem(descriptor.id);
            }

            ImGui::Separator();
            if (ImGui::MenuItem("Reset Layout"))
            {
                if (m_ctx && m_ctx->execute_command)
                    m_ctx->execute_command(EditorCommandId::ResetLayout);
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Edit"))
        {
            const bool can_undo = m_ctx && m_ctx->can_undo && m_ctx->can_undo();
            const bool can_redo = m_ctx && m_ctx->can_redo && m_ctx->can_redo();

            if (ImGui::MenuItem("Undo", "Ctrl+Z", false, can_undo) && m_ctx && m_ctx->undo)
                m_ctx->undo();

            if (ImGui::MenuItem("Redo", "Ctrl+Y", false, can_redo) && m_ctx && m_ctx->redo)
                m_ctx->redo();

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
            const std::string base = std::string(HYBRID_EDITOR_RESOURCES_DIR) + "/icons/";
            g_TopToolbarIcons.play = LoadTextureRGBA8(base + "icon_topTool_play.png");
            g_TopToolbarIcons.pause = LoadTextureRGBA8(base + "icon_topTool_pause.png");
            g_TopToolbarIcons.stop = LoadTextureRGBA8(base + "icon_topTool_stop.png");
            g_TopToolbarIcons.loaded =
                (g_TopToolbarIcons.play != 0 && g_TopToolbarIcons.pause != 0 && g_TopToolbarIcons.stop != 0);
            if (!g_TopToolbarIcons.loaded && !g_TopToolbarIconLoadFailedLogged)
            {
                HBD_CORE_WARN("{} top_toolbar_icon_load_failed", kEditorUILogTag);
                g_TopToolbarIconLoadFailedLogged = true;
            }
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
            status_line = m_ctx->active_document->display_name.empty() ? std::string("Untitled") : m_ctx->active_document->display_name;
        else
            status_line = "Untitled";

        if (m_ctx->active_document && m_ctx->active_document->dirty)
            status_line += " (Unsaved)";
        else if (m_ctx->active_document && m_ctx->active_document->isSaved())
            status_line += " (Saved)";

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
                if (m_ctx->execute_command &&
                    m_ctx->can_execute_command &&
                    m_ctx->can_execute_command(EditorCommandId::EnterPlayMode))
                {
                    HBD_CORE_INFO("{} play_mode_enter_requested", kEditorUILogTag);
                    m_ctx->execute_command(EditorCommandId::EnterPlayMode);
                }
            }
            else if (m_ctx->execute_command &&
                     m_ctx->can_execute_command &&
                     m_ctx->can_execute_command(EditorCommandId::ExitPlayMode))
            {
                HBD_CORE_INFO("{} play_mode_exit_requested", kEditorUILogTag);
                m_ctx->execute_command(EditorCommandId::ExitPlayMode);
            }
        }

        ImGui::SameLine();
        const bool pause_enabled = m_ctx && m_ctx->can_execute_command &&
            m_ctx->can_execute_command(EditorCommandId::TogglePauseMode);
        if (!pause_enabled)
            ImGui::BeginDisabled();
        if (drawTopToolbarButton("##TopToolbarPause", g_TopToolbarIcons.pause, is_paused ? "Resume" : "Pause", is_paused ? "Resume" : "Pause", is_paused, false) &&
            m_ctx->execute_command)
        {
            HBD_CORE_INFO("{} play_mode_pause_toggle_requested paused={}", kEditorUILogTag, !is_paused);
            m_ctx->execute_command(EditorCommandId::TogglePauseMode);
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

        std::array<ImGuiID, static_cast<size_t>(EditorLayoutNode::Count)> nodes{};
        nodes[static_cast<size_t>(EditorLayoutNode::Main)] = m_DockSpaceID;

        for (const EditorLayoutSplitDesc& split : kDefaultLayoutSplits)
        {
            ImGuiID& source = nodes[static_cast<size_t>(split.source)];
            ImGuiID& primary = nodes[static_cast<size_t>(split.out_primary)];
            ImGuiID& secondary = nodes[static_cast<size_t>(split.out_secondary)];
            ImGui::DockBuilderSplitNode(source, toImGuiDir(split.direction), split.ratio, &primary, &secondary);
        }

        for (const EditorPanelDescriptor& descriptor : kPanelDescriptors)
        {
            const char* window_name = getPanelWindowName(descriptor.id);
            ImGuiID dock_target = nodes[static_cast<size_t>(EditorLayoutNode::Main)];
            switch (descriptor.default_dock_slot)
            {
            case EditorDockSlot::Right:
                dock_target = nodes[static_cast<size_t>(EditorLayoutNode::Right)];
                break;
            case EditorDockSlot::LeftBottom:
                dock_target = nodes[static_cast<size_t>(EditorLayoutNode::LeftBottom)];
                break;
            case EditorDockSlot::LeftTopLeft:
                dock_target = nodes[static_cast<size_t>(EditorLayoutNode::LeftTopLeft)];
                break;
            case EditorDockSlot::Main:
            default:
                dock_target = nodes[static_cast<size_t>(EditorLayoutNode::Main)];
                break;
            }
            ImGui::DockBuilderDockWindow(window_name, dock_target);
        }

        ImGui::DockBuilderFinish(m_DockSpaceID);
    }

    const EditorPanelDescriptor* EditorUI::getPanelDescriptor(EditorPanelId id) const
    {
        for (const EditorPanelDescriptor& descriptor : kPanelDescriptors)
        {
            if (descriptor.id == id)
                return &descriptor;
        }
        return nullptr;
    }

    IEditorPanel* EditorUI::getPanel(EditorPanelId id) const
    {
        const size_t index = static_cast<size_t>(id);
        return index < m_panels.size() ? m_panels[index] : nullptr;
    }

    void EditorUI::drawPanelToggleMenuItem(EditorPanelId id)
    {
        IEditorPanel* panel = getPanel(id);
        const EditorPanelDescriptor* descriptor = getPanelDescriptor(id);
        if (panel == nullptr || descriptor == nullptr)
            return;

        bool open = panel->isOpen();
        if (ImGui::MenuItem(descriptor->title, nullptr, &open))
            panel->setOpen(open);
    }

    const char* EditorUI::getPanelWindowName(EditorPanelId id) const
    {
        IEditorPanel* panel = getPanel(id);
        return panel ? panel->getName() : "";
    }

    void EditorUI::renderViewportPanel(EditorPanelId id, uint32_t colorTexID)
    {
        if (!m_ctx)
            return;

        switch (id)
        {
        case EditorPanelId::SceneView:
            if (m_SceneViewportPanel && m_SceneViewportPanel->isOpen())
            {
                m_SceneViewportPanel->setTexture(colorTexID);
                m_SceneViewportPanel->onImGuiRender(*m_ctx);
            }
            break;
        case EditorPanelId::GameView:
            if (m_GameViewportPanel && m_GameViewportPanel->isOpen())
            {
                m_GameViewportPanel->setTexture(colorTexID);
                m_GameViewportPanel->onImGuiRender(*m_ctx);
            }
            break;
        default:
            break;
        }
    }

    void EditorUI::updateViewportPanelState(EditorPanelId id)
    {
        if (!m_ctx)
            return;

        switch (id)
        {
        case EditorPanelId::SceneView:
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
            break;
        case EditorPanelId::GameView:
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
            break;
        default:
            break;
        }
    }

    void EditorUI::drawConfirmDialogs()
    {
        if (!m_active_confirm_dialog.has_value() && !m_confirm_dialog_queue.empty())
        {
            m_active_confirm_dialog = std::move(m_confirm_dialog_queue.front());
            m_confirm_dialog_queue.pop_front();
            m_OpenConfirmDialog = true;
        }

        if (!m_active_confirm_dialog.has_value())
            return;

        constexpr const char* kConfirmDialogPopupId = "##EditorConfirmDialog";
        if (m_OpenConfirmDialog)
        {
            ImGui::OpenPopup(kConfirmDialogPopupId);
            m_OpenConfirmDialog = false;
        }

        if (!ImGui::BeginPopupModal(kConfirmDialogPopupId, nullptr, ImGuiWindowFlags_AlwaysAutoResize))
            return;

        EditorConfirmDialog dialog = *m_active_confirm_dialog;
        ImGui::TextUnformatted(dialog.title.c_str());
        ImGui::Separator();
        ImGui::PushTextWrapPos(420.0f);
        ImGui::TextUnformatted(dialog.message.c_str());
        ImGui::PopTextWrapPos();
        ImGui::Dummy(ImVec2(0.0f, 8.0f));

        bool close_dialog = false;
        if (ImGui::Button(dialog.confirm_label.c_str(), ImVec2(140.0f, 0.0f)))
        {
            if (dialog.on_confirm)
                dialog.on_confirm();
            close_dialog = true;
        }

        if (!dialog.secondary_label.empty())
        {
            ImGui::SameLine();
            if (ImGui::Button(dialog.secondary_label.c_str(), ImVec2(140.0f, 0.0f)))
            {
                if (dialog.on_secondary)
                    dialog.on_secondary();
                close_dialog = true;
            }
        }

        ImGui::SameLine();
        if (ImGui::Button(dialog.cancel_label.c_str(), ImVec2(140.0f, 0.0f)))
        {
            if (dialog.on_cancel)
                dialog.on_cancel();
            close_dialog = true;
        }

        if (close_dialog)
        {
            m_active_confirm_dialog.reset();
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
} // namespace Hybrid
