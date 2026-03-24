#pragma once

#include <string>
#include <vector>

#include <entt/entt.hpp>

#include "editor/core/transform_snapshot.h"
#include "runtime/modules/scene/components.h"

namespace Hybrid
{
    class Scene;

    struct EntitySnapshot
    {
        UUID id{};
        std::string tag = "Entity";
        TransformSnapshot transform{};

        bool has_camera = false;
        CameraComponent camera{};

        bool has_mesh_renderer = false;
        MeshRendererComponent mesh_renderer{};

        bool has_directional_light = false;
        DirectionalLightComponent directional_light{};

        bool has_point_light = false;
        PointLightComponent point_light{};

        bool has_collider = false;
        ColliderComponent collider{};

        bool has_rigidbody = false;
        RigidbodyComponent rigidbody{};

        std::vector<EntitySnapshot> children;
    };

    EntitySnapshot CaptureEntity(Scene& scene, entt::entity entity);
    EntitySnapshot CaptureEntitySubtree(Scene& scene, entt::entity root);
    bool ApplyEntitySnapshot(Scene& scene, entt::entity entity, const EntitySnapshot& snapshot);
    entt::entity RestoreEntitySubtree(Scene& scene, const EntitySnapshot& snapshot, entt::entity parent = entt::null);
    EntitySnapshot CloneEntitySnapshotWithFreshUUIDs(const EntitySnapshot& snapshot);
} // namespace Hybrid
