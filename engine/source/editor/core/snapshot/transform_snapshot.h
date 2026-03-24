#pragma once

#include <entt/entity/fwd.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/vec3.hpp>

namespace Hybrid
{
    class Scene;
    struct TransformComponent;

    struct TransformSnapshot
    {
        glm::vec3 position{0.0f, 0.0f, 0.0f};
        glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
        glm::vec3 scale{1.0f, 1.0f, 1.0f};
    };

    TransformSnapshot CaptureTransformSnapshot(const TransformComponent& transform);
    bool TransformSnapshotsEqual(const TransformSnapshot& lhs, const TransformSnapshot& rhs, float epsilon = 1e-5f);
    bool ApplyTransformSnapshot(Scene& scene, entt::entity entity, const TransformSnapshot& snapshot);
} // namespace Hybrid
