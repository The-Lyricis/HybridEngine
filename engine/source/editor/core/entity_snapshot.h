#pragma once

#include <string>
#include <vector>

#include <entt/entt.hpp>

#include "editor/core/transform_snapshot.h"
#include "runtime/modules/scene/component_schema.h"
#include "runtime/modules/scene/components.h"

namespace Hybrid
{
    class Scene;

    struct EntitySnapshot
    {
        UUID id{};
        std::string tag = "Entity";
        TransformSnapshot transform{};

#define HYBRID_DECLARE_SNAPSHOT_COMPONENT(id, type, member, name, key, flags) \
        bool has_##member = false; \
        type member{};
        HYBRID_OPTIONAL_SCENE_COMPONENTS(HYBRID_DECLARE_SNAPSHOT_COMPONENT)
#undef HYBRID_DECLARE_SNAPSHOT_COMPONENT

        std::vector<EntitySnapshot> children;
    };

    EntitySnapshot CaptureEntity(Scene& scene, entt::entity entity);
    EntitySnapshot CaptureEntitySubtree(Scene& scene, entt::entity root);
    bool ApplyEntitySnapshot(Scene& scene, entt::entity entity, const EntitySnapshot& snapshot);
    entt::entity RestoreEntitySubtree(Scene& scene, const EntitySnapshot& snapshot, entt::entity parent = entt::null);
    EntitySnapshot CloneEntitySnapshotWithFreshUUIDs(const EntitySnapshot& snapshot);
} // namespace Hybrid
