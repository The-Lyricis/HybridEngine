#pragma once

#include <optional>
#include <string_view>

namespace Hybrid
{
    enum class GraphicsBackend
    {
        Null = 0,
        OpenGL,
        Metal,
        Vulkan,
    };

    constexpr const char* ToString(GraphicsBackend backend)
    {
        switch (backend)
        {
        case GraphicsBackend::Null:   return "null";
        case GraphicsBackend::OpenGL: return "opengl";
        case GraphicsBackend::Metal:  return "metal";
        case GraphicsBackend::Vulkan: return "vulkan";
        }
        return "unknown";
    }

    inline std::optional<GraphicsBackend> ParseGraphicsBackend(std::string_view value)
    {
        if (value == "opengl" || value == "gl")
            return GraphicsBackend::OpenGL;
        if (value == "metal")
            return GraphicsBackend::Metal;
        if (value == "vulkan" || value == "vk")
            return GraphicsBackend::Vulkan;
        return std::nullopt;
    }
} // namespace Hybrid
