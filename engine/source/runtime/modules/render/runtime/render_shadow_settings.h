#pragma once

#include <array>
#include <cstdint>

namespace Hybrid
{
    inline constexpr uint32_t kMaxDirectionalShadowCascades = 4;

    struct DirectionalShadowSettings
    {
        uint32_t map_resolution = 2048;
        uint32_t cascade_count = 1;
        std::array<float, kMaxDirectionalShadowCascades> cascade_split_ratios{0.10f, 0.25f, 0.50f, 1.0f};
        float near_distance = 0.1f;
        float far_distance = 60.0f;
        float light_distance = 50.0f;
        float receiver_margin_xy = 10.0f;
        float projection_margin_xy = 4.0f;
        float caster_back_padding = 15.0f;
        float receiver_front_padding = 2.0f;
        float strength = 1.0f;
        float bias_constant = 0.0005f;
        float bias_slope = 0.0015f;
    };
} // namespace Hybrid
