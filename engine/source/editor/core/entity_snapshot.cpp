#include "entity_snapshot.h"

#include "runtime/modules/scene/entity.h"
#include "runtime/modules/scene/scene.h"

namespace Hybrid
{
    namespace
    {
        void CaptureEntitySubtreeRecursive(Scene& scene, entt::entity entity, EntitySnapshot& snapshot)
        {
            auto& registry = scene.getRegistry();
            const Entity wrapped(entity, &registry, &scene);

            snapshot.id = wrapped.GetComponent<IDComponent>().ID;
            snapshot.tag = wrapped.GetComponent<TagComponent>().Tag;
            snapshot.transform = CaptureTransformSnapshot(wrapped.GetComponent<TransformComponent>());

            snapshot.has_camera = wrapped.HasComponent<CameraComponent>();
            if (snapshot.has_camera)
                snapshot.camera = wrapped.GetComponent<CameraComponent>();

            snapshot.has_mesh_renderer = wrapped.HasComponent<MeshRendererComponent>();
            if (snapshot.has_mesh_renderer)
                snapshot.mesh_renderer = wrapped.GetComponent<MeshRendererComponent>();

            snapshot.has_directional_light = wrapped.HasComponent<DirectionalLightComponent>();
            if (snapshot.has_directional_light)
                snapshot.directional_light = wrapped.GetComponent<DirectionalLightComponent>();

            snapshot.has_point_light = wrapped.HasComponent<PointLightComponent>();
            if (snapshot.has_point_light)
                snapshot.point_light = wrapped.GetComponent<PointLightComponent>();

            snapshot.has_collider = wrapped.HasComponent<ColliderComponent>();
            if (snapshot.has_collider)
                snapshot.collider = wrapped.GetComponent<ColliderComponent>();

            snapshot.has_rigidbody = wrapped.HasComponent<RigidbodyComponent>();
            if (snapshot.has_rigidbody)
                snapshot.rigidbody = wrapped.GetComponent<RigidbodyComponent>();

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

        entt::entity RestoreEntitySubtreeRecursive(Scene& scene, const EntitySnapshot& snapshot, entt::entity parent)
        {
            Entity restored = scene.createEntityWithUUID(snapshot.id, snapshot.tag);
            auto& transform = restored.GetComponent<TransformComponent>();
            transform.Position = snapshot.transform.position;
            transform.Rotation = snapshot.transform.rotation;
            transform.Scale = snapshot.transform.scale;
            transform.DirtyLocal = true;
            transform.DirtyWorld = true;

            if (snapshot.has_camera)
                restored.AddComponent<CameraComponent>(snapshot.camera);

            if (snapshot.has_mesh_renderer)
                restored.AddComponent<MeshRendererComponent>(snapshot.mesh_renderer);

            if (snapshot.has_directional_light)
                restored.AddComponent<DirectionalLightComponent>(snapshot.directional_light);

            if (snapshot.has_point_light)
                restored.AddComponent<PointLightComponent>(snapshot.point_light);

            if (snapshot.has_collider)
                restored.AddComponent<ColliderComponent>(snapshot.collider);

            if (snapshot.has_rigidbody)
                restored.AddComponent<RigidbodyComponent>(snapshot.rigidbody);

            if (parent != entt::null)
                scene.SetParent(restored, Entity(parent, &scene.getRegistry(), &scene), false);

            for (auto it = snapshot.children.rbegin(); it != snapshot.children.rend(); ++it)
                RestoreEntitySubtreeRecursive(scene, *it, restored.GetHandle());

            scene.MarkDirtyRecursive(restored);
            return restored.GetHandle();
        }
    } // namespace

    EntitySnapshot CaptureEntitySubtree(Scene& scene, entt::entity root)
    {
        EntitySnapshot snapshot{};
        CaptureEntitySubtreeRecursive(scene, root, snapshot);
        return snapshot;
    }

    entt::entity RestoreEntitySubtree(Scene& scene, const EntitySnapshot& snapshot, entt::entity parent)
    {
        return RestoreEntitySubtreeRecursive(scene, snapshot, parent);
    }
} // namespace Hybrid
