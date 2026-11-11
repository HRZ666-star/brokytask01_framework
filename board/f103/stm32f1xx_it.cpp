#include "board.h"

extern "C" void SysTick_Handler(void) {
    HAL_IncTick();
}

extern "C" void USART1_IRQHandler(void) {
    HAL_UART_IRQHandler(&huart1);
}

extern "C" void TIM2_IRQHandler(void) {
    HAL_TIM_IRQHandler(&htim2);
}

extern "C" void I2C1_EV_IRQHandler(void) {
    HAL_I2C_EV_IRQHandler(&hi2c1);
}

extern "C" void I2C1_ER_IRQHandler(void) {
    HAL_I2C_ER_IRQHandler(&hi2c1);
}

extern "C" void USB_HP_CAN1_TX_IRQHandler(void) {
    HAL_CAN_IRQHandler(&hcan1);
}

extern "C" void USB_LP_CAN1_RX0_IRQHandler(void) {
    HAL_CAN_IRQHandler(&hcan1);
}

extern "C" void CAN1_RX1_IRQHandler(void) {
    HAL_CAN_IRQHandler(&hcan1);
}

extern "C" void CAN1_SCE_IRQHandler(void) {
    HAL_CAN_IRQHandler(&hcan1);
}
