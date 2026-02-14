#pragma once
#include "../renderer_api.h"

namespace Hybrid {

    // GLRendererAPI: RendererAPI backed by modern OpenGL.
    class GLRendererAPI final : public RendererAPI {
    public:
        void init() override;
        void setViewport(int x, int y, int width, int height) override;
        void setClearColor(const glm::vec4& color) override;
        void clear() override;
        void drawIndexed(uint32_t indexCount) override;
    };

} // namespace Hybrid
