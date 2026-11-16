#pragma once

#include "bsp/interface/pwm/pwm.h"

/*
 * Board-injected PWM instance.  Application/module code includes this header
 * (not the STM32 adapter) and talks to bsp::IPwm only.
 */
bsp::IPwm& board_pwm();
