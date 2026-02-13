#pragma once
#include <string>
#include <cstdint>
#include <glm/glm.hpp>

namespace Hybrid
{
    // 可选：后续接入真正 UUID 系统（现在先用 uint64_t 占位）
    struct IDComponent
    {
        uint64_t ID = 0;
    };

    struct TagComponent
    {
        std::string Tag;
    };

    struct TransformComponent
    {
        glm::vec3 Position{ 0.0f, 0.0f, 0.0f };
        glm::vec3 Rotation{ 0.0f, 0.0f, 0.0f }; // Euler，后续可换 quaternion
        glm::vec3 Scale{ 1.0f, 1.0f, 1.0f };
    };
}
