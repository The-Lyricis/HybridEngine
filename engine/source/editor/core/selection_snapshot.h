#pragma once

#include <vector>

#include "runtime/modules/scene/uuid.h"

namespace Hybrid
{
    class Scene;
    struct EditorContext;

    struct EditorSelectionSnapshot
    {
        std::vector<UUID> items;
        UUID active{};
        UUID range_anchor{};
    };

    EditorSelectionSnapshot CaptureSelectionSnapshot(const EditorContext& ctx, const Scene* scene);
    void RestoreSelectionSnapshot(EditorContext& ctx, Scene* scene, const EditorSelectionSnapshot& snapshot);
} // namespace Hybrid
