#include "entity_snapshot.h"

#include "runtime/modules/scene/entity.h"
#include "runtime/modules/scene/scene.h"

namespace Hybrid
{
    namespace
    {
        void CaptureEntityData(Scene& scene, entt::entity entity, EntitySnapshot& snapshot)
        {
            auto& registry = scene.getRegistry();
            const Entity wrapped(entity, &registry, &scene);

            snapshot.id = wrapped.GetComponent<IDComponent>().ID;
            snapshot.tag = wrapped.GetComponent<TagComponent>().Tag;
            snapshot.transform = CaptureTransformSnapshot(wrapped.GetComponent<TransformComponent>());

#define HYBRID_CAPTURE_SNAPSHOT_COMPONENT(id, type, member, name, key, flags) \
            snapshot.has_##member = wrapped.HasComponent<type>(); \
            if (snapshot.has_##member) \
                snapshot.member = wrapped.GetComponent<type>();
            HYBRID_OPTIONAL_SCENE_COMPONENTS(HYBRID_CAPTURE_SNAPSHOT_COMPONENT)
#undef HYBRID_CAPTURE_SNAPSHOT_COMPONENT
        }

        void CaptureEntitySubtreeRecursive(Scene& scene, entt::entity entity, EntitySnapshot& snapshot)
        {
            auto& registry = scene.getRegistry();
            const Entity wrapped(entity, &registry, &scene);

            CaptureEntityData(scene, entity, snapshot);

            const auto& transform = wrapped.GetComponent<TransformComponent>();
            for (entt::entity child = transform.FirstChild; child != entt::null;)
            {
                if (!registry.valid(child) || !registry.all_of<TransformComponent>(child))
                    break;

                const entt::entity next = registry.get<TransformComponent>(child).NextSibling;
                snapshot.children.emplace_back();
                CaptureEntitySubtreeRecursive(scene, child, snapshot.children.back());
                child = next;
            }
        }

        template<typename TComponent>
        void ApplyOptionalComponent(Entity entity, bool should_have, const TComponent& value)
        {
            if (should_have)
            {
                if (entity.HasComponent<TComponent>())
                    entity.GetComponent<TComponent>() = value;
                else
                    entity.AddComponent<TComponent>(value);
            }
            else if (entity.HasComponent<TComponent>())
            {
                entity.RemoveComponent<TComponent>();
            }
        }

        entt::entity RestoreEntitySubtreeRecursive(Scene& scene, const EntitySnapshot& snapshot, entt::entity parent)
        {
            Entity restored = scene.createEntityWithUUID(snapshot.id, snapshot.tag);
            auto& transform = restored.GetComponent<TransformComponent>();
            transform.Position = snapshot.transform.position;
            transform.Rotation = snapshot.transform.rotation;
            transform.Scale = snapshot.transform.scale;
            transform.DirtyLocal = true;
            transform.DirtyWorld = true;

#define HYBRID_RESTORE_SNAPSHOT_COMPONENT(id, type, member, name, key, flags) \
            if (snapshot.has_##member) \
                restored.AddComponent<type>(snapshot.member);
            HYBRID_OPTIONAL_SCENE_COMPONENTS(HYBRID_RESTORE_SNAPSHOT_COMPONENT)
#undef HYBRID_RESTORE_SNAPSHOT_COMPONENT

            if (parent != entt::null)
                scene.SetParent(restored, Entity(parent, &scene.getRegistry(), &scene), false);

            for (auto it = snapshot.children.rbegin(); it != snapshot.children.rend(); ++it)
                RestoreEntitySubtreeRecursive(scene, *it, restored.GetHandle());

            scene.MarkDirtyRecursive(restored);
            return restored.GetHandle();
        }
    } // namespace

    EntitySnapshot CaptureEntity(Scene& scene, entt::entity entity)
    {
        EntitySnapshot snapshot{};
        CaptureEntityData(scene, entity, snapshot);
        return snapshot;
    }

    EntitySnapshot CaptureEntitySubtree(Scene& scene, entt::entity root)
    {
        EntitySnapshot snapshot{};
        CaptureEntitySubtreeRecursive(scene, root, snapshot);
        return snapshot;
    }

    bool ApplyEntitySnapshot(Scene& scene, entt::entity entity_handle, const EntitySnapshot& snapshot)
    {
        auto& registry = scene.getRegistry();
        if (entity_handle == entt::null || !registry.valid(entity_handle))
            return false;

        Entity entity(entity_handle, &registry, &scene);
        if (!entity.HasComponent<IDComponent>() || entity.GetComponent<IDComponent>().ID != snapshot.id)
            return false;

        entity.GetComponent<TagComponent>().Tag = snapshot.tag;

        auto& transform = entity.GetComponent<TransformComponent>();
        transform.Position = snapshot.transform.position;
        transform.Rotation = snapshot.transform.rotation;
        transform.Scale = snapshot.transform.scale;
        transform.DirtyLocal = true;
        transform.DirtyWorld = true;

#define HYBRID_APPLY_SNAPSHOT_COMPONENT(id, type, member, name, key, flags) \
        ApplyOptionalComponent<type>(entity, snapshot.has_##member, snapshot.member);
        HYBRID_OPTIONAL_SCENE_COMPONENTS(HYBRID_APPLY_SNAPSHOT_COMPONENT)
#undef HYBRID_APPLY_SNAPSHOT_COMPONENT

        scene.MarkDirtyRecursive(entity);
        return true;
    }

    entt::entity RestoreEntitySubtree(Scene& scene, const EntitySnapshot& snapshot, entt::entity parent)
    {
        return RestoreEntitySubtreeRecursive(scene, snapshot, parent);
    }

    EntitySnapshot CloneEntitySnapshotWithFreshUUIDs(const EntitySnapshot& snapshot)
    {
        EntitySnapshot copy = snapshot;
        copy.id = UUIDGenerator::New();
        for (EntitySnapshot& child : copy.children)
            child = CloneEntitySnapshotWithFreshUUIDs(child);
        return copy;
    }
} // namespace Hybrid
