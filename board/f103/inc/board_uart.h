#pragma once

#include "bsp/interface/uart/uart.h"

/*
 * Board-injected UART instance.  Application code includes this header (not
 * the STM32 adapter) and talks to bsp::IUart only.
 */
bsp::IUart& board_uart();
