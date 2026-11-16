#pragma once

#include "bsp/interface/i2c/i2c.h"

/*
 * Board-injected I2C instance.  Application code includes this header (not
 * the STM32 adapter) and talks to bsp::II2c only.
 */
bsp::II2c& board_i2c();
