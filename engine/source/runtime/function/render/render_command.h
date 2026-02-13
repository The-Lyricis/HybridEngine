#pragma once
#include <glm/glm.hpp>
#include <memory>
#include "renderer_api.h"

namespace Hybrid {

    // RenderCommand: thin static wrapper around the active RendererAPI.
    class RenderCommand {
    public:
        static void initialize();
        static void setViewport(int x, int y, int width, int height);
        static void setClearColor(const glm::vec4& color);
        static void clear();

        static void drawIndexed(unsigned int indexCount);

    private:
        static std::unique_ptr<RendererAPI> s_API;
    };

} // namespace Hybrid
