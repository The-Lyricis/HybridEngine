#pragma once

#include <glm/vec4.hpp>

namespace Hybrid
{
    struct SelectionOverlayStyle
    {
        float depth_epsilon = 1e-5f;
        glm::vec4 visible_outline_color{0.836f, 0.292f, 0.312f, 0.95f};
        glm::vec4 occluded_outline_color{0.320f, 0.360f, 0.500f, 0.40f};
        glm::vec4 fill_color{0.836f, 0.292f, 0.312f, 0.12f};
    };
} // namespace Hybrid
