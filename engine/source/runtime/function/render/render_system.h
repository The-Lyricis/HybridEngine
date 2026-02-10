#pragma once
#include <memory>
#include <cstdint>
#include <glm/vec2.hpp>

#include "runtime/function/input/input_state.h"
#include "runtime/function/render/editor_camera.h" // 你自己的相机类头文件路径

namespace Hybrid {

    class Framebuffer;
    class VertexArray;
    class Shader;

    class RenderSystem {
    public:
        RenderSystem() = default;
        ~RenderSystem() = default;

        void initialize(void* glfwWindowHandle);

        uint32_t getSceneColorTexture() const;

        //输入收敛为 InputState
        void renderFrame(const glm::vec2& viewportSize,
            void* glfwWindowHandle,
            float dt,
            bool viewportActive,
            const InputState& input);

    private:
        void createCubeResources();
        void ensureFramebufferSize(uint32_t w, uint32_t h);

    private:
        std::shared_ptr<Framebuffer> m_SceneFB;

        std::shared_ptr<VertexArray> m_CubeVAO;
        std::shared_ptr<Shader>      m_CubeShader;

        EditorCamera m_Camera;

        bool m_Initialized = false;
    };

} // namespace Hybrid
