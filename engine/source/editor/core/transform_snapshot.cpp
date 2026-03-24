#include "transform_snapshot.h"

#include <cmath>

#include "runtime/core/base/math_util.h"
#include "runtime/modules/scene/components/transform_component.h"
#include "runtime/modules/scene/entity.h"
#include "runtime/modules/scene/scene.h"

namespace Hybrid
{
    TransformSnapshot CaptureTransformSnapshot(const TransformComponent& transform)
    {
        TransformSnapshot snapshot{};
        snapshot.position = transform.Position;
        snapshot.rotation = transform.Rotation;
        snapshot.scale = transform.Scale;
        return snapshot;
    }

    bool TransformSnapshotsEqual(const TransformSnapshot& lhs, const TransformSnapshot& rhs, float epsilon)
    {
        const auto vec_near = [epsilon](const glm::vec3& a, const glm::vec3& b)
        {
            return MathUtil::nearlyEqual(a.x, b.x, epsilon) &&
                   MathUtil::nearlyEqual(a.y, b.y, epsilon) &&
                   MathUtil::nearlyEqual(a.z, b.z, epsilon);
        };

        const float rotation_dot = std::abs(glm::dot(lhs.rotation, rhs.rotation));
        const bool rotation_equal = MathUtil::nearlyEqual(rotation_dot, 1.0f, epsilon);
        return vec_near(lhs.position, rhs.position) &&
               vec_near(lhs.scale, rhs.scale) &&
               rotation_equal;
    }

    bool ApplyTransformSnapshot(Scene& scene, entt::entity entity, const TransformSnapshot& snapshot)
    {
        auto& registry = scene.getRegistry();
        if (entity == entt::null || !registry.valid(entity) || !registry.all_of<TransformComponent>(entity))
            return false;

        auto& transform = registry.get<TransformComponent>(entity);
        transform.Position = snapshot.position;
        transform.Rotation = MathUtil::normalizeQuat(snapshot.rotation);
        transform.Scale = snapshot.scale;
        transform.DirtyLocal = true;

        scene.MarkDirtyRecursive(Entity(entity, &registry, &scene));
        return true;
    }
} // namespace Hybrid
