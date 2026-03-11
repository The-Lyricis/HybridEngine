#pragma once

#include <glm/vec3.hpp>

namespace Hybrid
{
    struct RigidbodyComponent
    {
        bool Enabled = true;
        glm::vec3 Velocity{0.0f, 0.0f, 0.0f};
        glm::vec3 Force{0.0f, 0.0f, 0.0f};

        float Mass = 1.0f;
        bool UseGravity = true;
        bool IsKinematic = false;
    };
} // namespace Hybrid
