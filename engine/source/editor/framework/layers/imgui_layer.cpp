#include "imgui_layer.h"

#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

#include "runtime/core/base/macro.h"

namespace Hybrid
{
    namespace
    {
        constexpr const char* kImGuiLayerLogTag = "[ImGuiLayer]";
    } // namespace

    ImGuiLayer::ImGuiLayer(GLFWwindow* window) : Layer("ImGuiLayer"), m_window(window) {}

    void ImGuiLayer::onAttach()
    {
        if (!m_window)
        {
            HBD_CORE_ERROR("{} attach_failed reason=window_is_null", kImGuiLayerLogTag);
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
        HBD_CORE_INFO("{} attach_completed", kImGuiLayerLogTag);
    }

    void ImGuiLayer::onDetach()
    {
        if (!m_initialized)
            return;

        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        m_initialized = false;
        HBD_CORE_INFO("{} detach_completed", kImGuiLayerLogTag);
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
