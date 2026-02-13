#pragma once
#include <cstdint>
#include <memory>
#include <glm/vec4.hpp>
/**
 * RendererAPI: abstract rendering backend interface.
 * Concrete implementations live in platform-specific files
 * (e.g. OpenGLRendererAPI). Upper layers should depend only
 * on this interface.
 */
namespace Hybrid {

    class RendererAPI {
    public:
        enum class API { None = 0, OpenGL = 1 };

        virtual ~RendererAPI() = default;

        virtual void init() = 0;
        virtual void setViewport(int x, int y, int width, int height) = 0;
        virtual void setClearColor(const glm::vec4& color) = 0;
        virtual void clear() = 0;
        virtual void drawIndexed(uint32_t indexCount) = 0;

        static API getAPI();
        static std::unique_ptr<RendererAPI> Create();
    };

} // namespace Hybrid
