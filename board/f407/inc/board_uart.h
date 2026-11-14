#pragma once

#include "bsp/interface/uart/uart.h"

#ifdef __cplusplus
extern "C" {
#endif

bsp::IUart& board_uart();

#ifdef __cplusplus
}
#endif
