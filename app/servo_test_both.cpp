/*
 * Two-axis servo bring-up: yaw on PA6 / TIM3_CH1, pitch on PA7 / TIM3_CH2.
 *
 * Wiring:
 *   yaw   brown -> GND (common with STM32)
 *         red   -> external 5 V (not 3.3 V)
 *         orange-> PA6
 *   pitch brown -> GND
 *         red   -> external 5 V
 *         orange-> PA7
 *
 * The sweep is deliberately limited to +/-30 deg and moves one axis at a
 * time.  This proves the second PWM channel and the mechanical assembly
 * before a control loop is allowed to command larger angles.
 */
#include "board.h"
#include "board_pwm.h"
#include "modules/servo/servo.h"

#include <cstdio>

namespace {

constexpr std::uint16_t kMinPulseUs = 500U;
constexpr std::uint16_t kMaxPulseUs = 2500U;
constexpr std::uint32_t kTimerClockHz = 72000000U;
constexpr std::int16_t kSweepLimitDeg = 30;
constexpr std::uint32_t kStepDelayMs = 300U;

common::Error init_servo(bsp::IPwm& pwm, modules::Servo& servo,
                         const char* name) {
    const common::Error pwm_error =
        pwm.init({50U, kMinPulseUs, kMaxPulseUs, kTimerClockHz});
    if (pwm_error != common::Error::Ok) {
        std::printf("servo both: %s pwm init failed (%d)\r\n", name,
                    static_cast<int>(pwm_error));
        return pwm_error;
    }

    const common::Error servo_error = servo.init(
        pwm, {kMinPulseUs, kMaxPulseUs, -90, 90});
    if (servo_error != common::Error::Ok) {
        std::printf("servo both: %s servo init failed (%d)\r\n", name,
                    static_cast<int>(servo_error));
    }
    return servo_error;
}

void center(modules::Servo& servo, const char* name) {
    const common::Error error = servo.center();
    if (error != common::Error::Ok) {
        std::printf("servo both: %s center failed (%d)\r\n", name,
                    static_cast<int>(error));
        Error_Handler();
    }
    std::printf("servo both: %s center pulse=%uus\r\n", name,
                static_cast<unsigned>(servo.pulseUs()));
}

void sweep(modules::Servo& servo, const char* name, std::int16_t from,
           std::int16_t to) {
    const std::int16_t step = (to >= from) ? 10 : -10;
    for (std::int16_t angle = from;; angle = static_cast<std::int16_t>(
                                     angle + step)) {
        const common::Error error = servo.setAngleDeg(angle);
        if (error != common::Error::Ok) {
            std::printf("servo both: %s set %d failed (%d)\r\n", name, angle,
                        static_cast<int>(error));
            Error_Handler();
        }
        std::printf("servo both: %s angle=%d pulse=%uus\r\n", name, angle,
                    static_cast<unsigned>(servo.pulseUs()));
        HAL_Delay(kStepDelayMs);
        if (angle == to) {
            break;
        }
    }
}

}  // namespace

int main(void) {
    BOARD_INIT();

    modules::Servo yaw;
    modules::Servo pitch;

    if (init_servo(board_pwm(), yaw, "yaw") != common::Error::Ok ||
        init_servo(board_pwm_pitch(), pitch, "pitch") != common::Error::Ok) {
        Error_Handler();
    }

    std::printf("servo both: yaw=PA6 pitch=PA7, 50Hz, +/-30deg\r\n");

    center(yaw, "yaw");
    center(pitch, "pitch");
    HAL_Delay(800U);

    for (;;) {
        std::printf("servo both: sweep yaw\r\n");
        center(pitch, "pitch");
        sweep(yaw, "yaw", -kSweepLimitDeg, kSweepLimitDeg);
        sweep(yaw, "yaw", kSweepLimitDeg, -kSweepLimitDeg);
        center(yaw, "yaw");
        HAL_Delay(500U);

        std::printf("servo both: sweep pitch\r\n");
        center(yaw, "yaw");
        sweep(pitch, "pitch", -kSweepLimitDeg, kSweepLimitDeg);
        sweep(pitch, "pitch", kSweepLimitDeg, -kSweepLimitDeg);
        center(pitch, "pitch");
        HAL_Delay(500U);
    }
}
