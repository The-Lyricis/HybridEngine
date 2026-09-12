#pragma once

#include <imgui.h>

#include "editor/core/context/editor_context.h"
#include "editor/services/render/editor_image_handle.h"

namespace Hybrid
{
    struct SceneViewViewportState
    {
        bool missing_texture_logged = false;
    };

    struct SceneViewViewportResult
    {
        ImVec2 canvas_size{1.0f, 1.0f};
        ImVec2 viewport_min{0.0f, 0.0f};
        ImVec2 viewport_max{0.0f, 0.0f};
        bool hovered = false;
    };

    SceneViewViewportResult DrawSceneViewViewport(EditorContext& ctx,
                                                  EditorImageHandle image,
                                                  bool toolbar_interacted,
                                                  SceneViewViewportState& state,
                                                  const char* log_tag);
} // namespace Hybrid
