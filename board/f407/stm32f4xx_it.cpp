#include "board.h"

extern "C" {

void NMI_Handler(void) {
    for (;;) {
    }
}

void HardFault_Handler(void) {
    for (;;) {
    }
}

void MemManage_Handler(void) {
    for (;;) {
    }
}

void BusFault_Handler(void) {
    for (;;) {
    }
}

void UsageFault_Handler(void) {
    for (;;) {
    }
}

void SVC_Handler(void) {
    for (;;) {
    }
}

void DebugMon_Handler(void) {
    for (;;) {
    }
}

void PendSV_Handler(void) {
    for (;;) {
    }
}

void SysTick_Handler(void) {
    HAL_IncTick();
}

void USART1_IRQHandler(void) {
    HAL_UART_IRQHandler(&huart1);
}

void TIM2_IRQHandler(void) {
    HAL_TIM_IRQHandler(&htim2);
}

}
