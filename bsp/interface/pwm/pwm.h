#pragma once

#include "common/error/error.h"

#include <cstdint>

namespace bsp {

/*
 * Generic PWM output.  The servo module only needs a pulse width in
 * microseconds; the adapter owns the timer prescaler/period details.
 */
struct PwmConfig {
    std::uint32_t frequency_hz{50U};
    std::uint16_t min_pulse_us{500U};
    std::uint16_t max_pulse_us{2500U};
    std::uint32_t timer_clock_hz{72000000U};
};

class IPwm {
public:
    virtual ~IPwm() = default;

    [[nodiscard]] virtual common::Error init(const PwmConfig& config) = 0;
    [[nodiscard]] virtual common::Error setPulseUs(std::uint16_t pulse_us) = 0;
    [[nodiscard]] virtual std::uint16_t pulseUs() const noexcept = 0;
    [[nodiscard]] virtual const PwmConfig& config() const noexcept = 0;
};

}  // namespace bsp
