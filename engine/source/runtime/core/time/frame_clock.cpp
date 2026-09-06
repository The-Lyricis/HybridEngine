#include "frame_clock.h"

#include <algorithm>
#include <cmath>

namespace Hybrid
{
    void FrameClock::configure(const FrameClockConfig& config)
    {
        m_fixed_step = 1.0f / std::max(1.0f, config.fixed_update_hz);
        m_max_fixed_steps = std::max<uint32_t>(1, config.max_fixed_steps);
        reset();
    }

    void FrameClock::reset()
    {
        m_accumulator = 0.0f;
    }

    FrameAdvanceResult FrameClock::advance(float frame_dt, const std::function<void(float)>& fixed_update)
    {
        FrameAdvanceResult result{};
        m_accumulator += std::max(0.0f, frame_dt);
        while (m_accumulator >= m_fixed_step && result.fixed_steps < m_max_fixed_steps)
        {
            if (fixed_update)
                fixed_update(m_fixed_step);
            m_accumulator -= m_fixed_step;
            ++result.fixed_steps;
        }
        if (m_accumulator >= m_fixed_step)
        {
            m_accumulator = std::fmod(m_accumulator, m_fixed_step);
            result.dropped_time = true;
        }
        return result;
    }
} // namespace Hybrid
