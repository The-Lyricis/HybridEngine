#pragma once

#include "editor/core/context/editor_context.h"

namespace Hybrid
{
    struct SceneViewToolbarResult
    {
        float height = 0.0f;
        bool interacted = false;
    };

    SceneViewToolbarResult DrawSceneViewToolbar(EditorContext& ctx, float available_width);
} // namespace Hybrid
