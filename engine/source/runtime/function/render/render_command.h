#pragma once
#include <glm/glm.hpp>

namespace Hybrid {

    class RenderCommand {
    public:
        static void initialize();
        static void setViewport(int x, int y, int width, int height);
        static void setClearColor(const glm::vec4& color);
        static void clear();

        static void drawIndexed(unsigned int indexCount);
    };

} // namespace Hybrid
