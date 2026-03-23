#pragma once
#include "runtime/modules/render/public/renderer_api.h"

namespace Hybrid {

    // GLRendererAPI: RendererAPI backed by modern OpenGL.
    class GLRendererAPI final : public RendererAPI {
    public:
        void init() override;
        void setViewport(int x, int y, int width, int height) override;
        void setClearColor(const glm::vec4& color) override;
        void setClearDepth(float depth) override;
        void clear() override;
        void setBlendEnabled(bool enabled) override;
        void setDepthTestEnabled(bool enabled) override;
        void setCullEnabled(bool enabled) override;
        void setDepthWriteEnabled(bool enabled) override;
        void setDepthCompareFunc(DepthCompareFunc func) override;
        void setLineWidth(float width) override;
        void drawIndexed(uint32_t indexCount, uint32_t indexOffset = 0) override;
        void drawLinesIndexed(uint32_t indexCount, uint32_t indexOffset = 0) override;
    };

} // namespace Hybrid


