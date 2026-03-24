#pragma once

#include <entt/entt.hpp>
#include <imgui.h>

#include "editor/core/context/editor_context.h"
#include "editor/core/snapshot/transform_snapshot.h"

namespace Hybrid
{
    struct SceneViewGizmoDragState
    {
        bool drag_active = false;
        entt::entity drag_entity = entt::null;
        TransformSnapshot drag_before{};
    };

    struct SceneViewGizmoResult
    {
        bool using_gizmo = false;
        bool gizmo_over = false;
    };

    SceneViewGizmoResult HandleSceneViewGizmo(EditorContext& ctx,
                                              const ImVec2& viewport_min,
                                              const ImVec2& canvas_size,
                                              SceneViewGizmoDragState& drag_state);
} // namespace Hybrid
