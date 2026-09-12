#include "imgui_layer.h"

#include "editor/services/render/imgui_render_backend.h"
#include "runtime/core/base/macro.h"

namespace Hybrid
{
    namespace
    {
        constexpr const char* kImGuiLayerLogTag = "[ImGuiLayer]";
    } // namespace

    ImGuiLayer::ImGuiLayer(IWindow& window, std::shared_ptr<IImGuiRenderBackend> backend)
        : Layer("ImGuiLayer"), m_window(&window), m_backend(std::move(backend)) {}
    ImGuiLayer::~ImGuiLayer() = default;

    void ImGuiLayer::onAttach()
    {
        if (!m_window)
        {
            HBD_CORE_ERROR("{} attach_failed reason=window_is_null", kImGuiLayerLogTag);
            return;
        }

        if (!m_backend)
        {
            HBD_CORE_ERROR("{} attach_failed reason=unsupported_backend backend={}",
                           kImGuiLayerLogTag, ToString(m_window->graphicsBackend()));
            return;
        }
        std::string error;
        if (!m_backend->initialize(*m_window, error))
        {
            HBD_CORE_ERROR("{} attach_failed reason={}", kImGuiLayerLogTag, error);
            return;
        }
        m_initialized = true;
        HBD_CORE_INFO("{} attach_completed backend={}",
                      kImGuiLayerLogTag, ToString(m_window->graphicsBackend()));
    }

    void ImGuiLayer::onDetach()
    {
        if (!m_initialized)
            return;

        m_backend->shutdown();
        m_initialized = false;
        HBD_CORE_INFO("{} detach_completed", kImGuiLayerLogTag);
    }

    void ImGuiLayer::onBeginFrame()
    {
        if (!m_initialized)
            return;

        m_backend->newFrame();
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

        m_backend->render();
    }
} // namespace Hybrid
