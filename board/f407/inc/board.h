#pragma once

#include "common/error/error.h"
#include "stm32f4xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

/* FK407M3-VET6: user LED is PC13 on most boards; verify against the silkscreen. */
#define LED_Pin        GPIO_PIN_13
#define LED_GPIO_Port  GPIOC

extern UART_HandleTypeDef huart1;
extern TIM_HandleTypeDef  htim2;

common::Error board_init(void);
void board_led_write(uint8_t on);
void board_led_toggle(void);
uint32_t board_timer_ticks(void);
void Error_Handler(void);

#ifdef __cplusplus
}  /* extern "C" */
#endif

#ifdef __cplusplus
[[noreturn]] void fatal_error(common::Error error, const char* file, int line);

inline void board_init_checked(const char* file, int line) {
    const common::Error error = board_init();
    if (error != common::Error::Ok) {
        fatal_error(error, file, line);
    }
}

#define BOARD_INIT() board_init_checked(__FILE__, __LINE__)
#else
#define BOARD_INIT() board_init()
#endif
