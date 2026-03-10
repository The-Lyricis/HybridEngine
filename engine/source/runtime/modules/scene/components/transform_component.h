#pragma once

#include <entt/entt.hpp>
#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Hybrid
{
    struct TransformComponent
    {
        glm::vec3 Position{0.0f, 0.0f, 0.0f};
        glm::quat Rotation{1.0f, 0.0f, 0.0f, 0.0f};
        glm::vec3 Scale{1.0f, 1.0f, 1.0f};

        entt::entity Parent = entt::null;
        entt::entity FirstChild = entt::null;
        entt::entity NextSibling = entt::null;
        entt::entity PrevSibling = entt::null;

        glm::mat4 LocalMatrix{1.0f};
        glm::mat4 WorldMatrix{1.0f};
        bool DirtyLocal = true;
        bool DirtyWorld = true;
    };
} // namespace Hybrid
