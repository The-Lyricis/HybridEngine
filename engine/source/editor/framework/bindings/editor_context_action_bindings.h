#pragma once

#include "editor/core/context/editor_context.h"

namespace Hybrid
{
    struct EditorContextActionBindings
    {
        EditorDocumentActions documents;
        EditorSceneActions scene_actions;
        EditorAssetActions asset_actions;
        EditorCommandActions commands;
        EditorModeActions mode;
    };

    void BindEditorContextActions(EditorContext& ctx, EditorContextActionBindings bindings);
    void ClearEditorContextActions(EditorContext& ctx);
} // namespace Hybrid
