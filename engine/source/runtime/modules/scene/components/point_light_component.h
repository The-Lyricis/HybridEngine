#pragma once

#include <glm/vec3.hpp>

namespace Hybrid
{
    struct PointLightComponent
    {
        glm::vec3 Color{1.0f, 1.0f, 1.0f};
        float Intensity = 1.0f;
        float Range = 10.0f;
    };
} // namespace Hybrid
