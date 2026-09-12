#pragma once
#include <cstdint>
#include <memory>
#include <glm/vec4.hpp>

#include "runtime/core/platform/graphics_backend.h"
/**
 * RendererAPI: abstract rendering backend interface.
 * Concrete implementations live in platform-specific files
 * (e.g. GLRendererAPI). Upper layers should depend only
 * on this interface.
 */
namespace Hybrid {

    enum class DepthCompareFunc : uint8_t
    {
        Less = 0,
        LessEqual,
    };

    class RendererAPI {
    public:
        using API = GraphicsBackend;

        virtual ~RendererAPI() = default;

        virtual void init() = 0;
        virtual void setViewport(int x, int y, int width, int height) = 0;
        virtual void setClearColor(const glm::vec4& color) = 0;
        virtual void setClearDepth(float depth) = 0;
        virtual void clear() = 0;
        virtual void setBlendEnabled(bool enabled) = 0;
        virtual void setDepthTestEnabled(bool enabled) = 0;
        virtual void setCullEnabled(bool enabled) = 0;
        virtual void setDepthWriteEnabled(bool enabled) = 0;
        virtual void setDepthCompareFunc(DepthCompareFunc func) = 0;
        virtual void setLineWidth(float width) = 0;
        virtual void drawIndexed(uint32_t indexCount, uint32_t indexOffset = 0) = 0;
        virtual void drawLinesIndexed(uint32_t indexCount, uint32_t indexOffset = 0) = 0;

        static API getAPI();
        static bool setAPI(API api);
        static bool isSupported(API api);
        static std::unique_ptr<RendererAPI> Create();
    };

} // namespace Hybrid
