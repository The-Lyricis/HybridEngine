#pragma once

#include <glm/vec3.hpp>

namespace Hybrid
{
    struct AABB
    {
        glm::vec3 Min{0.0f, 0.0f, 0.0f};
        glm::vec3 Max{0.0f, 0.0f, 0.0f};
    };

    struct CollisionHit
    {
        bool Hit = false;
        glm::vec3 Normal{0.0f, 0.0f, 0.0f};
        float Penetration = 0.0f;
    };
}
