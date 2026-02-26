#include "imgui_layer.h"

#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

#include "runtime/core/base/macro.h"

namespace Hybrid
{
    ImGuiLayer::ImGuiLayer(GLFWwindow* window) : Layer("ImGuiLayer"), m_window(window) {}

    void ImGuiLayer::onAttach()
    {
        if (!m_window)
        {
            HBD_CORE_ERROR("ImGuiLayer attach failed: window is null");
            return;
        }

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        ImGui::StyleColorsDark();

        ImGui_ImplGlfw_InitForOpenGL(m_window, true);
        ImGui_ImplOpenGL3_Init("#version 330");
        m_initialized = true;
    }

    void ImGuiLayer::onDetach()
    {
        if (!m_initialized)
            return;

        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        m_initialized = false;
    }

    void ImGuiLayer::onBeginFrame()
    {
        if (!m_initialized)
            return;

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
    }

    void ImGuiLayer::onUpdate(float /*dt*/)
    {
    }

    void ImGuiLayer::onImGuiRender()
    {
    }

    void ImGuiLayer::onEndFrame()
    {
        if (!m_initialized)
            return;

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }
} // namespace Hybrid
