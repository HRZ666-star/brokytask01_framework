/*
 * SG90 / 50 Hz servo bring-up.
 *
 * Wiring:
 *   brown  -> GND (common with STM32)
 *   red    -> external 5 V (not 3.3 V)
 *   orange -> PA6 / TIM3_CH1
 *
 * The sweep is intentionally slow (300 ms per 10 deg) so the mechanical
 * direction and endpoints can be checked before closing a control loop.
 */
#include "board.h"
#include "board_pwm.h"
#include "modules/servo/servo.h"

#include <cstdio>

int main(void) {
    BOARD_INIT();

    const common::Error pwm_error =
        board_pwm().init({50U, 500U, 2500U, 72000000U});
    if (pwm_error != common::Error::Ok) {
        std::printf("servo test: pwm init failed (%d)\r\n",
                    static_cast<int>(pwm_error));
        Error_Handler();
    }

    modules::Servo servo;
    const common::Error servo_error =
        servo.init(board_pwm(), {500U, 2500U, -90, 90});
    if (servo_error != common::Error::Ok) {
        std::printf("servo test: servo init failed (%d)\r\n",
                    static_cast<int>(servo_error));
        Error_Handler();
    }

    std::printf("servo test: 50Hz, 500-2500us, -90..+90deg\r\n");

    for (;;) {
        for (int angle = -90; angle <= 90; angle += 10) {
            const common::Error error =
                servo.setAngleDeg(static_cast<std::int16_t>(angle));
            if (error != common::Error::Ok) {
                std::printf("servo test: set %d failed (%d)\r\n", angle,
                            static_cast<int>(error));
                Error_Handler();
            }
            std::printf("servo test: angle=%d pulse=%uus\r\n", angle,
                        static_cast<unsigned>(servo.pulseUs()));
            HAL_Delay(300U);
        }
        for (int angle = 90; angle >= -90; angle -= 10) {
            const common::Error error =
                servo.setAngleDeg(static_cast<std::int16_t>(angle));
            if (error != common::Error::Ok) {
                std::printf("servo test: set %d failed (%d)\r\n", angle,
                            static_cast<int>(error));
                Error_Handler();
            }
            std::printf("servo test: angle=%d pulse=%uus\r\n", angle,
                        static_cast<unsigned>(servo.pulseUs()));
            HAL_Delay(300U);
        }
    }
}
