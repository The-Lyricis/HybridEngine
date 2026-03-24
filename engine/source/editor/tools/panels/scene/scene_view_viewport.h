#pragma once

#include <cstdint>

#include <imgui.h>

#include "editor/core/context/editor_context.h"

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
                                                  uint32_t color_texture_id,
                                                  bool toolbar_interacted,
                                                  SceneViewViewportState& state,
                                                  const char* log_tag);
} // namespace Hybrid
