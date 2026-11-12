/*
 * FreeRTOS bring-up smoke test.
 *
 * Goal: prove that the Cortex-M3 port, FreeRTOSConfig.h, the linker script
 * and the vector table all agree before any BSP interface is written.
 *
 * Expected result: PC13 blinks at 1 Hz and the UART prints two boot lines.
 * This is intentionally a separate application (APP=freertos_smoke) so the
 * normal LED/UART demo in app/main.cpp stays untouched.
 */
#include "board.h"

#include "FreeRTOS.h"
#include "task.h"

#include <cstdio>

namespace {

constexpr uint32_t kBlinkPeriodMs = 500;
constexpr uint32_t kBlinkStackWords = 128;   // 512 bytes on Cortex-M3
constexpr UBaseType_t kBlinkPriority = 2;

void blink_task(void* argument) {
    (void)argument;

    for (;;) {
        board_led_toggle();
        vTaskDelay(pdMS_TO_TICKS(kBlinkPeriodMs));
    }
}

}  // namespace

int main(void) {
    board_init();

    std::printf("freertos smoke: boot, sysclk=%lu\r\n",
                static_cast<unsigned long>(HAL_RCC_GetHCLKFreq()));

    const BaseType_t created =
        xTaskCreate(blink_task, "blink", kBlinkStackWords, nullptr,
                    kBlinkPriority, nullptr);
    if (created != pdPASS) {
        std::printf("freertos smoke: xTaskCreate failed (heap too small?)\r\n");
        Error_Handler();
    }

    std::printf("freertos smoke: starting scheduler\r\n");

    /*
     * HAL_Init() started SysTick for HAL_Delay().  Stop its interrupt until
     * the scheduler has initialised the FreeRTOS lists; otherwise the first
     * tick can call xPortSysTickHandler() before pxDelayedTaskList exists.
     * vTaskStartScheduler() reprograms SysTick and enables it again.
     */
    SysTick->CTRL &= ~SysTick_CTRL_TICKINT_Msk;
    SCB->ICSR = SCB_ICSR_PENDSTCLR_Msk;

    vTaskStartScheduler();

    /* vTaskStartScheduler() only returns if there is not enough heap for the
     * idle/timer tasks, or if the port failed to start.  Never continue. */
    std::printf("freertos smoke: scheduler returned unexpectedly\r\n");
    Error_Handler();
}
