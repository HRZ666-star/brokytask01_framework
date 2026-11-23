#pragma once

#include "bsp/interface/pwm/pwm.h"

#include "stm32f1xx_hal.h"

namespace bsp {

/*
 * STM32 timer PWM adapter.  The board layer only configures the GPIO, clock
 * and htim.Instance; this adapter owns the 1 us prescaler and the channel.
 */
class HalPwm final : public IPwm {
public:
    HalPwm(TIM_HandleTypeDef& timer, std::uint32_t channel) noexcept;

    [[nodiscard]] common::Error init(const PwmConfig& config) override;
    [[nodiscard]] common::Error setPulseUs(std::uint16_t pulse_us) override;
    [[nodiscard]] std::uint16_t pulseUs() const noexcept override;
    [[nodiscard]] const PwmConfig& config() const noexcept override;

private:
    TIM_HandleTypeDef& timer_;
    std::uint32_t channel_;
    PwmConfig config_{};
    std::uint16_t pulse_us_{0U};
    bool initialized_{false};
};

}  // namespace bsp
