#pragma once

#include <cstdint>
#include <functional>

namespace Hybrid
{
    struct FrameClockConfig
    {
        float fixed_update_hz = 60.0f;
        uint32_t max_fixed_steps = 4;
    };

    struct FrameAdvanceResult
    {
        uint32_t fixed_steps = 0;
        bool dropped_time = false;
    };

    class FrameClock
    {
    public:
        void configure(const FrameClockConfig& config = {});
        void reset();
        FrameAdvanceResult advance(float frame_dt, const std::function<void(float)>& fixed_update);

        float fixedStep() const { return m_fixed_step; }
        float accumulator() const { return m_accumulator; }

    private:
        float m_fixed_step = 1.0f / 60.0f;
        float m_accumulator = 0.0f;
        uint32_t m_max_fixed_steps = 4;
    };
} // namespace Hybrid
