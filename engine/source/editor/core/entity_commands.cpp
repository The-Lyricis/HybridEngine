#include "entity_commands.h"

#include "editor/core/editor_context.h"
#include "editor/core/scene_document.h"
#include "runtime/modules/scene/entity.h"
#include "runtime/modules/scene/scene.h"

namespace Hybrid
{
    namespace
    {
        UUID GetEntityUUID(Scene& scene, entt::entity entity)
        {
            auto& registry = scene.getRegistry();
            if (entity == entt::null || !registry.valid(entity) || !registry.all_of<IDComponent>(entity))
                return {};
            return registry.get<IDComponent>(entity).ID;
        }
    }

    CreateEntityCommand::CreateEntityCommand(std::shared_ptr<SceneDocument> document,
                                             EntitySnapshot snapshot,
                                             entt::entity parent,
                                             EditorSelectionSnapshot before_selection)
        : m_document(std::move(document))
        , m_snapshot(std::move(snapshot))
        , m_before_selection(std::move(before_selection))
    {
        if (auto doc = m_document.lock())
        {
            if (doc->scene)
                m_parent_id = GetEntityUUID(*doc->scene, parent);
        }
    }

    void CreateEntityCommand::undo(EditorContext& ctx)
    {
        auto document = lockDocument();
        if (!document || !document->scene)
            return;

        Entity entity = document->scene->findEntityByUUID(m_snapshot.id);
        if (entity)
            document->scene->DestroyEntityRecursive(entity);

        document->dirty = true;
        RestoreSelectionSnapshot(ctx, document->scene.get(), m_before_selection);
        ctx.markSceneDirty();
    }

    void CreateEntityCommand::redo(EditorContext& ctx)
    {
        auto document = lockDocument();
        if (!document || !document->scene)
            return;

        Entity entity = document->scene->findEntityByUUID(m_snapshot.id);
        if (!entity)
        {
            const entt::entity parent = resolveParent(*document->scene);
            const entt::entity restored = RestoreEntitySubtree(*document->scene, m_snapshot, parent);
            if (restored != entt::null)
                ctx.selection.setSingle(restored);
            else
                ctx.selection.clear();
        }
        else
        {
            ctx.selection.setSingle(entity.GetHandle());
        }

        document->dirty = true;
        ctx.markSceneDirty();
    }

    const char* CreateEntityCommand::name() const
    {
        return "Create Entity";
    }

    std::shared_ptr<SceneDocument> CreateEntityCommand::lockDocument() const
    {
        return m_document.lock();
    }

    entt::entity CreateEntityCommand::resolveParent(Scene& scene) const
    {
        if (m_parent_id.value == 0)
            return entt::null;
        const Entity parent = scene.findEntityByUUID(m_parent_id);
        return parent ? parent.GetHandle() : entt::null;
    }

    DeleteEntityCommand::DeleteEntityCommand(std::shared_ptr<SceneDocument> document,
                                             EntitySnapshot snapshot,
                                             entt::entity parent,
                                             EditorSelectionSnapshot before_selection,
                                             EditorSelectionSnapshot after_selection)
        : m_document(std::move(document))
        , m_snapshot(std::move(snapshot))
        , m_before_selection(std::move(before_selection))
        , m_after_selection(std::move(after_selection))
    {
        if (auto doc = m_document.lock())
        {
            if (doc->scene)
                m_parent_id = GetEntityUUID(*doc->scene, parent);
        }
    }

    void DeleteEntityCommand::undo(EditorContext& ctx)
    {
        auto document = lockDocument();
        if (!document || !document->scene)
            return;

        Entity entity = document->scene->findEntityByUUID(m_snapshot.id);
        if (!entity)
        {
            const entt::entity parent = resolveParent(*document->scene);
            (void)RestoreEntitySubtree(*document->scene, m_snapshot, parent);
        }

        document->dirty = true;
        RestoreSelectionSnapshot(ctx, document->scene.get(), m_before_selection);
        ctx.markSceneDirty();
    }

    void DeleteEntityCommand::redo(EditorContext& ctx)
    {
        auto document = lockDocument();
        if (!document || !document->scene)
            return;

        Entity entity = document->scene->findEntityByUUID(m_snapshot.id);
        if (entity)
            document->scene->DestroyEntityRecursive(entity);

        document->dirty = true;
        RestoreSelectionSnapshot(ctx, document->scene.get(), m_after_selection);
        ctx.markSceneDirty();
    }

    const char* DeleteEntityCommand::name() const
    {
        return "Delete Entity";
    }

    std::shared_ptr<SceneDocument> DeleteEntityCommand::lockDocument() const
    {
        return m_document.lock();
    }

    entt::entity DeleteEntityCommand::resolveParent(Scene& scene) const
    {
        if (m_parent_id.value == 0)
            return entt::null;
        const Entity parent = scene.findEntityByUUID(m_parent_id);
        return parent ? parent.GetHandle() : entt::null;
    }
} // namespace Hybrid
