#pragma once

#include <nlohmann/json_fwd.hpp>

#include "editor/framework/camera/editor_camera.h"

namespace Hybrid
{
    namespace EditorCameraStateSerde
    {
        nlohmann::json toJson(const EditorCameraState& state);
        bool fromJson(const nlohmann::json& root, EditorCameraState& out_state);
    } // namespace EditorCameraStateSerde
} // namespace Hybrid
