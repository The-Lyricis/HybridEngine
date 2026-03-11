#pragma once

#include <glm/vec4.hpp>

#include "runtime/modules/asset/asset_type.h"

namespace Hybrid
{
    struct MeshRendererComponent
    {
        bool Enabled = true;
        AssetID Mesh{};
        AssetID Material{};
        glm::vec4 Tint{1.0f};
    };
} // namespace Hybrid
