#include "servo.h"

#include <cstdint>

namespace modules {

common::Error Servo::init(bsp::IPwm& pwm, const ServoConfig& config) {
    if (config.min_pulse_us == 0U ||
        config.max_pulse_us <= config.min_pulse_us ||
        config.max_angle_deg <= config.min_angle_deg) {
        return common::Error::InvalidArgument;
    }

    pwm_ = &pwm;
    config_ = config;
    initialized_ = true;

    const common::Error center_error = center();
    if (center_error != common::Error::Ok) {
        initialized_ = false;
        pwm_ = nullptr;
        return center_error;
    }
    return common::Error::Ok;
}

common::Error Servo::setAngleDeg(std::int16_t angle_deg) {
    if (!initialized_ || pwm_ == nullptr) {
        return common::Error::NotInitialized;
    }
    if (angle_deg < config_.min_angle_deg ||
        angle_deg > config_.max_angle_deg) {
        return common::Error::InvalidArgument;
    }

    const std::int32_t angle_span =
        static_cast<std::int32_t>(config_.max_angle_deg) -
        static_cast<std::int32_t>(config_.min_angle_deg);
    const std::int32_t pulse_span =
        static_cast<std::int32_t>(config_.max_pulse_us) -
        static_cast<std::int32_t>(config_.min_pulse_us);
    const std::int32_t offset =
        static_cast<std::int32_t>(angle_deg) -
        static_cast<std::int32_t>(config_.min_angle_deg);
    const std::int32_t pulse =
        static_cast<std::int32_t>(config_.min_pulse_us) +
        (offset * pulse_span + angle_span / 2) / angle_span;

    const common::Error error =
        pwm_->setPulseUs(static_cast<std::uint16_t>(pulse));
    if (error == common::Error::Ok) {
        angle_deg_ = angle_deg;
    }
    return error;
}

common::Error Servo::center() {
    if (!initialized_ || pwm_ == nullptr) {
        return common::Error::NotInitialized;
    }
    const std::int32_t midpoint =
        (static_cast<std::int32_t>(config_.min_angle_deg) +
         static_cast<std::int32_t>(config_.max_angle_deg)) /
        2;
    return setAngleDeg(static_cast<std::int16_t>(midpoint));
}

std::int16_t Servo::angleDeg() const noexcept {
    return angle_deg_;
}

std::uint16_t Servo::pulseUs() const noexcept {
    return (initialized_ && pwm_ != nullptr) ? pwm_->pulseUs() : 0U;
}

bool Servo::initialized() const noexcept {
    return initialized_;
}

}  // namespace modules
