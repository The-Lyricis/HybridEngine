#pragma once

#include <glm/vec3.hpp>

namespace Hybrid
{
    struct DirectionalLightComponent
    {
        glm::vec3 Color{1.0f, 1.0f, 1.0f};
        float Intensity = 1.0f;
    };
} // namespace Hybrid
