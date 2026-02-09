#pragma once
#include <memory>
#include <glm/glm.hpp>
#include "vertex_array.h"
#include "shader.h"


namespace Hybrid {

    class Renderer {
    public:
        static void Init();
        static void BeginFrame(const glm::vec4& clearColor);
        static void Submit(const std::shared_ptr<VertexArray>& va, const std::shared_ptr<Shader>& shader);
        static void EndFrame();
    };

} // namespace Hybrid
