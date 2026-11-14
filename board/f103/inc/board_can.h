#pragma once

#include "bsp/interface/can/can.h"

/*
 * Board-injected CAN instance.  Application code includes this header (not
 * the STM32 adapter) and talks to bsp::ICan only.
 */
bsp::ICan& board_can();
