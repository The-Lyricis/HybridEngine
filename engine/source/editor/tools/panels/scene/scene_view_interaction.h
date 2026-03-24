#pragma once

#include <imgui.h>

#include "editor/core/context/editor_context.h"

namespace Hybrid
{
    void HandleSceneViewDragDrop(EditorContext& ctx, const char* log_tag);
    void HandleSceneViewPicking(EditorContext& ctx,
                                const ImVec2& viewport_min,
                                const ImVec2& canvas_size,
                                bool toolbar_interacted,
                                bool gizmo_using,
                                bool gizmo_over,
                                const char* log_tag);
} // namespace Hybrid
