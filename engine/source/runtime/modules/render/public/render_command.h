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
        static void setClearDepth(float depth);
        static void clear();
        static void setBlendEnabled(bool enabled);
        static void setDepthTestEnabled(bool enabled);
        static void setCullEnabled(bool enabled);
        static void setDepthWriteEnabled(bool enabled);
        static void setDepthCompareFunc(DepthCompareFunc func);
        static void setLineWidth(float width);

        static void drawIndexed(unsigned int indexCount, unsigned int indexOffset = 0);
        static void drawLinesIndexed(unsigned int indexCount, unsigned int indexOffset = 0);

    private:
        static std::unique_ptr<RendererAPI> s_API;
    };

} // namespace Hybrid
