#include "transform_command.h"

#include "editor/core/editor_context.h"
#include "editor/core/scene_document.h"
#include "runtime/modules/scene/scene.h"

namespace Hybrid
{
    TransformCommand::TransformCommand(std::shared_ptr<SceneDocument> document,
                                       entt::entity entity,
                                       TransformSnapshot before,
                                       TransformSnapshot after)
        : m_document(std::move(document))
        , m_entity(entity)
        , m_before(std::move(before))
        , m_after(std::move(after))
    {
    }

    void TransformCommand::undo(EditorContext& ctx)
    {
        (void)apply(ctx, m_before);
    }

    void TransformCommand::redo(EditorContext& ctx)
    {
        (void)apply(ctx, m_after);
    }

    const char* TransformCommand::name() const
    {
        return "Transform";
    }

    bool TransformCommand::apply(EditorContext& ctx, const TransformSnapshot& snapshot) const
    {
        const std::shared_ptr<SceneDocument> document = m_document.lock();
        if (!document || !document->scene)
            return false;

        if (!ApplyTransformSnapshot(*document->scene, m_entity, snapshot))
            return false;

        document->dirty = true;
        if (ctx.active_document == document)
            ctx.markSceneDirty();
        return true;
    }
} // namespace Hybrid
