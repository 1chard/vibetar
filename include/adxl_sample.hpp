#pragma once
#include "tilt_config.hpp"

namespace guitar
{
    // A short interruption must not release Tilt. A disconnected sensor must not
    // keep it pressed indefinitely. All axes are committed together after validation.
    struct AdxlSample
    {
        static constexpr uint32_t freshness_ms = 100;
        std::array<int16_t, 3> axes{};
        uint32_t updated = 0;
        bool received = false;
        constexpr bool fresh(uint32_t now) const
        {
            return received && uint32_t(now - updated) < freshness_ms;
        }
        constexpr bool accept(const std::array<uint8_t, 6> &bytes, uint32_t now)
        {
            std::array<int16_t, 3> next{};
            for (size_t i = 0; i < next.size(); ++i)
            {
                const auto raw = static_cast<int16_t>(uint16_t(bytes[2 * i]) | uint16_t(bytes[2 * i + 1]) << 8);
                if (raw < -4096 || raw > 4095)
                    return false;
                next[i] = adxl_mg(raw);
            }
            axes = next;
            updated = now;
            received = true;
            return true;
        }
    };
}
