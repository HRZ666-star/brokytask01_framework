#pragma once

#include "bsp/interface/pwm/pwm.h"
#include "common/error/error.h"

#include <cstdint>

namespace modules {

struct ServoConfig {
    std::uint16_t min_pulse_us{500U};
    std::uint16_t max_pulse_us{2500U};
    std::int16_t min_angle_deg{-90};
    std::int16_t max_angle_deg{90};
};

/*
 * Maps an angle to a 50 Hz PWM pulse.  The caller owns the IPwm instance and
 * must initialise it first; Servo only writes the pulse width.
 */
class Servo {
public:
    [[nodiscard]] common::Error init(bsp::IPwm& pwm, const ServoConfig& config);
    [[nodiscard]] common::Error setAngleDeg(std::int16_t angle_deg);
    [[nodiscard]] common::Error center();

    [[nodiscard]] std::int16_t angleDeg() const noexcept;
    [[nodiscard]] std::uint16_t pulseUs() const noexcept;
    [[nodiscard]] bool initialized() const noexcept;

private:
    bsp::IPwm* pwm_{nullptr};
    ServoConfig config_{};
    std::int16_t angle_deg_{0};
    bool initialized_{false};
};

}  // namespace modules
