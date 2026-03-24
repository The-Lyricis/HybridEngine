#include "selection_snapshot.h"

#include "editor/core/editor_context.h"
#include "runtime/modules/scene/entity.h"
#include "runtime/modules/scene/scene.h"

namespace Hybrid
{
    namespace
    {
        UUID GetEntityUUID(Scene* scene, entt::entity entity)
        {
            if (!scene || entity == entt::null)
                return {};

            auto& registry = scene->getRegistry();
            if (!registry.valid(entity) || !registry.all_of<IDComponent>(entity))
                return {};

            return registry.get<IDComponent>(entity).ID;
        }
    }

    EditorSelectionSnapshot CaptureSelectionSnapshot(const EditorContext& ctx, const Scene* scene)
    {
        EditorSelectionSnapshot snapshot{};
        if (!scene)
            return snapshot;

        snapshot.items.reserve(ctx.selection.items.size());
        for (entt::entity entity : ctx.selection.items)
        {
            const UUID id = GetEntityUUID(const_cast<Scene*>(scene), entity);
            if (id.value != 0)
                snapshot.items.push_back(id);
        }

        snapshot.active = GetEntityUUID(const_cast<Scene*>(scene), ctx.selection.active);
        snapshot.range_anchor = GetEntityUUID(const_cast<Scene*>(scene), ctx.selection.range_anchor);
        return snapshot;
    }

    void RestoreSelectionSnapshot(EditorContext& ctx, Scene* scene, const EditorSelectionSnapshot& snapshot)
    {
        ctx.selection.clear();
        if (!scene)
            return;

        ctx.selection.items.reserve(snapshot.items.size());
        for (const UUID id : snapshot.items)
        {
            if (id.value == 0)
                continue;

            const Entity entity = scene->findEntityByUUID(id);
            if (entity)
                ctx.selection.items.push_back(entity.GetHandle());
        }

        if (snapshot.active.value != 0)
        {
            const Entity active = scene->findEntityByUUID(snapshot.active);
            if (active)
                ctx.selection.active = active.GetHandle();
        }

        if (snapshot.range_anchor.value != 0)
        {
            const Entity anchor = scene->findEntityByUUID(snapshot.range_anchor);
            if (anchor)
                ctx.selection.range_anchor = anchor.GetHandle();
        }

        if (ctx.selection.items.empty())
            ctx.selection.clear();
    }
} // namespace Hybrid
