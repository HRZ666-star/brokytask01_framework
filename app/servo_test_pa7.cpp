/*
 * SG90 / 50 Hz servo bring-up on PA7 / TIM3_CH2.
 *
 * This image is deliberately separate from servo_test (PA6):
 *   - if PA7 moves but PA6 does not, the PA6 pin/wire is suspect;
 *   - if neither moves, the servo, 5 V rail or ground is suspect.
 *
 * Wiring:
 *   brown  -> GND (common with STM32)
 *   red    -> external 5 V (not 3.3 V)
 *   orange -> PA7 / TIM3_CH2
 */
#include "board.h"
#include "board_pwm.h"
#include "modules/servo/servo.h"

#include <cstdio>

int main(void) {
    BOARD_INIT();

    const common::Error pwm_error =
        board_pwm_pitch().init({50U, 500U, 2500U, 72000000U});
    if (pwm_error != common::Error::Ok) {
        std::printf("servo PA7 test: pwm init failed (%d)\r\n",
                    static_cast<int>(pwm_error));
        Error_Handler();
    }

    modules::Servo servo;
    const common::Error servo_error =
        servo.init(board_pwm_pitch(), {500U, 2500U, -90, 90});
    if (servo_error != common::Error::Ok) {
        std::printf("servo PA7 test: servo init failed (%d)\r\n",
                    static_cast<int>(servo_error));
        Error_Handler();
    }

    std::printf("servo PA7 test: 50Hz, 500-2500us, -90..+90deg\r\n");

    for (;;) {
        for (int angle = -90; angle <= 90; angle += 10) {
            const common::Error error =
                servo.setAngleDeg(static_cast<std::int16_t>(angle));
            if (error != common::Error::Ok) {
                std::printf("servo PA7 test: set %d failed (%d)\r\n", angle,
                            static_cast<int>(error));
                Error_Handler();
            }
            std::printf("servo PA7 test: angle=%d pulse=%uus\r\n", angle,
                        static_cast<unsigned>(servo.pulseUs()));
            HAL_Delay(300U);
        }
        for (int angle = 90; angle >= -90; angle -= 10) {
            const common::Error error =
                servo.setAngleDeg(static_cast<std::int16_t>(angle));
            if (error != common::Error::Ok) {
                std::printf("servo PA7 test: set %d failed (%d)\r\n", angle,
                            static_cast<int>(error));
                Error_Handler();
            }
            std::printf("servo PA7 test: angle=%d pulse=%uus\r\n", angle,
                        static_cast<unsigned>(servo.pulseUs()));
            HAL_Delay(300U);
        }
    }
}
