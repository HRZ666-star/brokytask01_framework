#pragma once

#include "bsp/interface/pwm/pwm.h"

/*
 * Board-injected PWM instance.  Application/module code includes this header
 * (not the STM32 adapter) and talks to bsp::IPwm only.
 */
bsp::IPwm& board_pwm();

/* PA7 / TIM3_CH2, added for the pitch servo bring-up. */
bsp::IPwm& board_pwm_pitch();
