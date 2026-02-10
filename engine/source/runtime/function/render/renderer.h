#pragma once
#include <memory>
#include <glm/glm.hpp>
#include "vertex_array.h"
#include "shader.h"


namespace Hybrid {

    class Renderer {
    public:
        static void initialize();
        static void beginFrame(const glm::vec4& clearColor);
        static void submit(const std::shared_ptr<VertexArray>& va, const std::shared_ptr<Shader>& shader);
        static void endFrame();
    };

} // namespace Hybrid
