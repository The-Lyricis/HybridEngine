#include "editor_context_action_bindings.h"

namespace Hybrid
{
    void BindEditorContextActions(EditorContext& ctx, EditorContextActionBindings bindings)
    {
        ctx.documents = std::move(bindings.documents);
        ctx.scene_actions = std::move(bindings.scene_actions);
        ctx.asset_actions = std::move(bindings.asset_actions);
        ctx.commands = std::move(bindings.commands);
        ctx.mode = std::move(bindings.mode);
        ctx.project = std::move(bindings.project);
    }

    void ClearEditorContextActions(EditorContext& ctx)
    {
        ctx.documents = {};
        ctx.scene_actions = {};
        ctx.asset_actions = {};
        ctx.commands = {};
        ctx.mode = {};
        ctx.project = {};
    }
} // namespace Hybrid
