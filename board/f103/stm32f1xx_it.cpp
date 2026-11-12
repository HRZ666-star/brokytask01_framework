#include "board.h"

#ifdef USE_FREERTOS
/*
 * FreeRTOS Cortex-M3 port entry points.  The port owns SVC, PendSV and the
 * SysTick *counter*, but the vector table still points at these names.
 */
extern "C" void vPortSVCHandler(void);
extern "C" void xPortPendSVHandler(void);
extern "C" void xPortSysTickHandler(void);

extern "C" void SVC_Handler(void) {
    vPortSVCHandler();
}

extern "C" void PendSV_Handler(void) {
    xPortPendSVHandler();
}
#endif

extern "C" void SysTick_Handler(void) {
    HAL_IncTick();
#ifdef USE_FREERTOS
    xPortSysTickHandler();
#endif
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
