#pragma once

#include <glm/vec3.hpp>

namespace Hybrid
{
    enum class ColliderType
    {
        None = 0,
        Box,
        Sphere
    };

    struct BoxColliderShape
    {
        glm::vec3 HalfExtents{ 0.5f, 0.5f, 0.5f };
    };

    struct SphereColliderShape
    {
        float Radius = 0.5f;
    };

    struct ColliderComponent
    {
        ColliderType Type = ColliderType::Box;

        bool IsTrigger = false;
        bool Enabled = true;

        glm::vec3 Center{ 0.0f, 0.0f, 0.0f };

        BoxColliderShape Box;
        SphereColliderShape Sphere;
    };
}
