#pragma once

#include "common/error/error.h"
#include "stm32f1xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Blue Pill: PC13, active low. Change here if your board differs. */
#define LED_Pin        GPIO_PIN_13
#define LED_GPIO_Port  GPIOC

extern UART_HandleTypeDef huart1;
extern TIM_HandleTypeDef  htim2;
extern TIM_HandleTypeDef  htim3;
extern I2C_HandleTypeDef  hi2c1;
extern CAN_HandleTypeDef  hcan1;

/*
 * Initialise the board and return the first failure instead of spinning in
 * Error_Handler().  Callers must check the return value; BOARD_INIT() below
 * is the checked convenience form used by the application images.
 */
common::Error board_init(void);
void board_led_write(uint8_t on);
void board_led_toggle(void);
uint32_t board_micros(void);

/* Legacy entry point kept for existing call sites; routes to fatal_error(). */
void Error_Handler(void);

#ifdef __cplusplus
}  /* extern "C" */
#endif

#ifdef __cplusplus
/*
 * Stop with a machine-readable error code and source location.  The values
 * are stored in globals so Ozone/J-Link can read them after the CPU halts.
 */
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
