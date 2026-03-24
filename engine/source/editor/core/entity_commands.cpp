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

    DuplicateEntityCommand::DuplicateEntityCommand(std::shared_ptr<SceneDocument> document,
                                                   std::vector<Entry> entries,
                                                   EditorSelectionSnapshot before_selection,
                                                   EditorSelectionSnapshot after_selection)
        : m_document(std::move(document))
        , m_entries(std::move(entries))
        , m_before_selection(std::move(before_selection))
        , m_after_selection(std::move(after_selection))
    {
    }

    void DuplicateEntityCommand::undo(EditorContext& ctx)
    {
        auto document = lockDocument();
        if (!document || !document->scene)
            return;

        for (const Entry& entry : m_entries)
        {
            Entity entity = document->scene->findEntityByUUID(entry.snapshot.id);
            if (entity)
                document->scene->DestroyEntityRecursive(entity);
        }

        document->dirty = true;
        RestoreSelectionSnapshot(ctx, document->scene.get(), m_before_selection);
        ctx.markSceneDirty();
    }

    void DuplicateEntityCommand::redo(EditorContext& ctx)
    {
        auto document = lockDocument();
        if (!document || !document->scene)
            return;

        for (const Entry& entry : m_entries)
        {
            Entity entity = document->scene->findEntityByUUID(entry.snapshot.id);
            if (!entity)
            {
                const entt::entity parent = resolveParent(*document->scene, entry.parent_id);
                (void)RestoreEntitySubtree(*document->scene, entry.snapshot, parent);
            }
        }

        document->dirty = true;
        RestoreSelectionSnapshot(ctx, document->scene.get(), m_after_selection);
        ctx.markSceneDirty();
    }

    const char* DuplicateEntityCommand::name() const
    {
        return "Duplicate Entity";
    }

    std::shared_ptr<SceneDocument> DuplicateEntityCommand::lockDocument() const
    {
        return m_document.lock();
    }

    entt::entity DuplicateEntityCommand::resolveParent(Scene& scene, const UUID& parent_id) const
    {
        if (parent_id.value == 0)
            return entt::null;
        const Entity parent = scene.findEntityByUUID(parent_id);
        return parent ? parent.GetHandle() : entt::null;
    }

    AddComponentCommand::AddComponentCommand(std::shared_ptr<SceneDocument> document,
                                             UUID entity_id,
                                             std::string component_name,
                                             EntitySnapshot before_snapshot,
                                             EntitySnapshot after_snapshot)
        : m_document(std::move(document))
        , m_entity_id(entity_id)
        , m_before_snapshot(std::move(before_snapshot))
        , m_after_snapshot(std::move(after_snapshot))
    {
        m_name = "Add ";
        m_name += std::move(component_name);
    }

    void AddComponentCommand::undo(EditorContext& ctx)
    {
        (void)apply(ctx, m_before_snapshot);
    }

    void AddComponentCommand::redo(EditorContext& ctx)
    {
        (void)apply(ctx, m_after_snapshot);
    }

    const char* AddComponentCommand::name() const
    {
        return m_name.c_str();
    }

    bool AddComponentCommand::apply(EditorContext& ctx, const EntitySnapshot& snapshot) const
    {
        auto document = lockDocument();
        if (!document || !document->scene)
            return false;

        Entity entity = document->scene->findEntityByUUID(m_entity_id);
        if (!entity)
            return false;

        if (!ApplyEntitySnapshot(*document->scene, entity.GetHandle(), snapshot))
            return false;

        document->dirty = true;
        ctx.markSceneDirty();
        return true;
    }

    std::shared_ptr<SceneDocument> AddComponentCommand::lockDocument() const
    {
        return m_document.lock();
    }

    RemoveComponentCommand::RemoveComponentCommand(std::shared_ptr<SceneDocument> document,
                                                   UUID entity_id,
                                                   std::string component_name,
                                                   EntitySnapshot before_snapshot,
                                                   EntitySnapshot after_snapshot)
        : m_document(std::move(document))
        , m_entity_id(entity_id)
        , m_before_snapshot(std::move(before_snapshot))
        , m_after_snapshot(std::move(after_snapshot))
    {
        m_name = "Remove ";
        m_name += std::move(component_name);
    }

    void RemoveComponentCommand::undo(EditorContext& ctx)
    {
        (void)apply(ctx, m_before_snapshot);
    }

    void RemoveComponentCommand::redo(EditorContext& ctx)
    {
        (void)apply(ctx, m_after_snapshot);
    }

    const char* RemoveComponentCommand::name() const
    {
        return m_name.c_str();
    }

    bool RemoveComponentCommand::apply(EditorContext& ctx, const EntitySnapshot& snapshot) const
    {
        auto document = lockDocument();
        if (!document || !document->scene)
            return false;

        Entity entity = document->scene->findEntityByUUID(m_entity_id);
        if (!entity)
            return false;

        if (!ApplyEntitySnapshot(*document->scene, entity.GetHandle(), snapshot))
            return false;

        document->dirty = true;
        ctx.markSceneDirty();
        return true;
    }

    std::shared_ptr<SceneDocument> RemoveComponentCommand::lockDocument() const
    {
        return m_document.lock();
    }
} // namespace Hybrid
