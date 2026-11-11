#pragma once

#include "stm32f1xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Blue Pill: PC13, active low. Change here if your board differs. */
#define LED_Pin        GPIO_PIN_13
#define LED_GPIO_Port  GPIOC

extern UART_HandleTypeDef huart1;
extern TIM_HandleTypeDef  htim2;
extern I2C_HandleTypeDef  hi2c1;
extern CAN_HandleTypeDef  hcan1;

void board_init(void);
void board_led_write(uint8_t on);
void board_led_toggle(void);
void Error_Handler(void);

#ifdef __cplusplus
}
#endif
