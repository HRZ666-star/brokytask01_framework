#include "hal_pwm.h"

#include <cstdint>

namespace bsp {
namespace {

common::Error map_hal_status(HAL_StatusTypeDef status) noexcept {
    switch (status) {
    case HAL_OK:
        return common::Error::Ok;
    case HAL_BUSY:
        return common::Error::Busy;
    case HAL_TIMEOUT:
        return common::Error::Timeout;
    case HAL_ERROR:
    default:
        return common::Error::Hardware;
    }
}

}  // namespace

HalPwm::HalPwm(TIM_HandleTypeDef& timer, std::uint32_t channel) noexcept
    : timer_(timer), channel_(channel) {}

common::Error HalPwm::init(const PwmConfig& config) {
    if (config.frequency_hz == 0U || config.timer_clock_hz == 0U ||
        config.min_pulse_us == 0U ||
        config.max_pulse_us <= config.min_pulse_us) {
        return common::Error::InvalidArgument;
    }

    const std::uint32_t ticks_per_us = config.timer_clock_hz / 1000000U;
    if (ticks_per_us == 0U || ticks_per_us > 0x10000U) {
        return common::Error::NotSupported;
    }

    const std::uint32_t period_us = 1000000U / config.frequency_hz;
    const std::uint32_t period_ticks = period_us;
    if (period_ticks == 0U || period_ticks > 0x10000U) {
        return common::Error::NotSupported;
    }
    if (config.max_pulse_us >= period_us) {
        return common::Error::InvalidArgument;
    }

    initialized_ = false;

    timer_.Init.Prescaler = ticks_per_us - 1U;
    timer_.Init.CounterMode = TIM_COUNTERMODE_UP;
    timer_.Init.Period = period_ticks - 1U;
    timer_.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    timer_.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;

    const common::Error init_error = map_hal_status(HAL_TIM_PWM_Init(&timer_));
    if (init_error != common::Error::Ok) {
        return init_error;
    }

    TIM_OC_InitTypeDef channel = {};
    channel.OCMode = TIM_OCMODE_PWM1;
    channel.Pulse = config.min_pulse_us;
    channel.OCPolarity = TIM_OCPOLARITY_HIGH;
    channel.OCFastMode = TIM_OCFAST_DISABLE;

    const common::Error channel_error = map_hal_status(
        HAL_TIM_PWM_ConfigChannel(&timer_, &channel, channel_));
    if (channel_error != common::Error::Ok) {
        return channel_error;
    }

    const common::Error start_error =
        map_hal_status(HAL_TIM_PWM_Start(&timer_, channel_));
    if (start_error != common::Error::Ok) {
        return start_error;
    }

    config_ = config;
    pulse_us_ = config.min_pulse_us;
    initialized_ = true;
    return common::Error::Ok;
}

common::Error HalPwm::setPulseUs(std::uint16_t pulse_us) {
    if (!initialized_) {
        return common::Error::NotInitialized;
    }
    if (pulse_us < config_.min_pulse_us || pulse_us > config_.max_pulse_us) {
        return common::Error::InvalidArgument;
    }

    __HAL_TIM_SET_COMPARE(&timer_, channel_, pulse_us);
    pulse_us_ = pulse_us;
    return common::Error::Ok;
}

std::uint16_t HalPwm::pulseUs() const noexcept {
    return pulse_us_;
}

const PwmConfig& HalPwm::config() const noexcept {
    return config_;
}

}  // namespace bsp
