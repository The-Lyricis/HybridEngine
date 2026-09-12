#include "editor/services/render/imgui_render_backend.h"

#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "runtime/core/platform/window.h"
#include "runtime/modules/render/backend/opengl/opengl_texture.h"
#include "runtime/modules/render/backend/opengl/opengl_render_device_bridge.h"
#include "runtime/modules/render/rhi/render_device.h"

namespace Hybrid
{
    namespace
    {
        class OpenGLImGuiRenderBackend final : public IImGuiRenderBackend
        {
        public:
            ~OpenGLImGuiRenderBackend() override
            {
                shutdown();
            }

            bool initialize(IWindow& window, std::string& error) override
            {
                if (m_initialized)
                    return true;
                if (window.graphicsBackend() != GraphicsBackend::OpenGL)
                {
                    error = "OpenGL ImGui backend requires an OpenGL window";
                    return false;
                }

                auto* glfw_window = static_cast<GLFWwindow*>(window.backendWindowHandle());
                if (!glfw_window)
                {
                    error = "OpenGL ImGui backend requires a GLFW window adapter";
                    return false;
                }

                IMGUI_CHECKVERSION();
                ImGui::CreateContext();
                ImGuiIO& io = ImGui::GetIO();
                io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
                io.ConfigWindowsMoveFromTitleBarOnly = true;
                ImGui::StyleColorsDark();

                if (!ImGui_ImplGlfw_InitForOpenGL(glfw_window, true))
                {
                    ImGui::DestroyContext();
                    error = "ImGui GLFW platform backend initialization failed";
                    return false;
                }
                if (!ImGui_ImplOpenGL3_Init("#version 330"))
                {
                    ImGui_ImplGlfw_Shutdown();
                    ImGui::DestroyContext();
                    error = "ImGui OpenGL renderer backend initialization failed";
                    return false;
                }

                m_initialized = true;
                return true;
            }

            void newFrame() override
            {
                if (!m_initialized)
                    return;
                ImGui_ImplOpenGL3_NewFrame();
                ImGui_ImplGlfw_NewFrame();
                ImGui::NewFrame();
            }

            void render() override
            {
                if (!m_initialized)
                    return;
                ImGui::Render();
                ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            }

            uint64_t registerTexture(const TexturePtr& texture, std::string& error) override
            {
                const auto gl_texture = std::dynamic_pointer_cast<GLTexture>(texture);
                if (!gl_texture)
                {
                    error = "OpenGL ImGui backend received a non-OpenGL texture";
                    return 0;
                }
                return gl_texture->id();
            }

            uint64_t registerTextureView(IRenderDevice& device,
                                         TextureViewHandle view,
                                         std::string& error) override
            {
                const auto result = ResolveOpenGLTextureNativeHandle(device, view);
                if (!result)
                {
                    error = result.error.message;
                    return 0;
                }
                return result.value;
            }

            void unregisterTexture(uint64_t) override
            {
                // The shared Texture owns the GL object; ImGui stores only its ID.
            }

            uint32_t maxTextureDimension2D() const override
            {
                GLint limit = 1;
                glGetIntegerv(GL_MAX_TEXTURE_SIZE, &limit);
                return static_cast<uint32_t>(limit > 0 ? limit : 1);
            }

            void shutdown() override
            {
                if (!m_initialized)
                    return;
                ImGui_ImplOpenGL3_Shutdown();
                ImGui_ImplGlfw_Shutdown();
                ImGui::DestroyContext();
                m_initialized = false;
            }

        private:
            bool m_initialized = false;
        };
    } // namespace

    std::unique_ptr<IImGuiRenderBackend> CreateImGuiRenderBackend(GraphicsBackend backend)
    {
        if (backend == GraphicsBackend::OpenGL)
            return std::make_unique<OpenGLImGuiRenderBackend>();
        return nullptr;
    }
} // namespace Hybrid
