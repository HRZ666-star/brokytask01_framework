/*
 * Async logger bring-up test.
 *
 * Expected serial output (115200 8N1):
 *   [0] [INFO] boot: logger test start
 *   [..] [INFO] test: heartbeat=0
 *   [..] [WARN] test: warn at 0
 *   [..] [ERROR] test: error at 0
 *   ...
 *   [..] [INFO] flood: dropped=...
 *
 * The flood task deliberately overruns the 16-entry queue so the drop path is
 * exercised; a drop must increment the counter, not block the producer.
 */
#include "board.h"
#include "board_uart.h"
#include "modules/logger/logger.h"

#include "FreeRTOS.h"
#include "task.h"

#include <cstdio>

namespace {

constexpr UBaseType_t kTestPriority = 2U;
constexpr configSTACK_DEPTH_TYPE kTestStackWords = 384U;

void test_task(void* argument) {
    (void)argument;

    std::uint32_t index = 0;
    for (;;) {
        LOG_INFO("test", "heartbeat=%lu", static_cast<unsigned long>(index));
        if ((index % 5U) == 0U) {
            LOG_WARN("test", "warn at %lu", static_cast<unsigned long>(index));
        }
        if ((index % 10U) == 0U) {
            LOG_ERROR("test", "error at %lu", static_cast<unsigned long>(index));
        }
        ++index;
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void flood_task(void* argument) {
    (void)argument;

    for (std::uint32_t index = 0; index < 200U; ++index) {
        LOG_INFO("flood", "message=%lu", static_cast<unsigned long>(index));
    }

    vTaskDelay(pdMS_TO_TICKS(2000));
    LOG_INFO("flood", "dropped=%lu write_errors=%lu",
             static_cast<unsigned long>(modules::logger().dropped()),
             static_cast<unsigned long>(modules::logger().write_errors()));

    /* Keep the task alive without relying on INCLUDE_vTaskDelete. */
    for (;;) {
        vTaskDelay(portMAX_DELAY);
    }
}

}  // namespace

int main(void) {
    board_init();
    bsp::IUart& uart = board_uart();

    if (modules::logger().init(uart) != common::Error::Ok) {
        Error_Handler();
    }

    std::printf("log test: boot, sysclk=%lu\r\n",
                static_cast<unsigned long>(HAL_RCC_GetHCLKFreq()));
    LOG_INFO("boot", "logger test start");

    if (xTaskCreate(test_task, "test", kTestStackWords, nullptr, kTestPriority,
                    nullptr) != pdPASS) {
        Error_Handler();
    }
    if (xTaskCreate(flood_task, "flood", kTestStackWords, nullptr, kTestPriority,
                    nullptr) != pdPASS) {
        Error_Handler();
    }

    /*
     * board_init() enabled SysTick for HAL_Delay().  Stop its interrupt and
     * clear any pending tick before handing the counter to FreeRTOS; otherwise
     * a tick can arrive while vTaskStartScheduler() is still setting up the
     * kernel lists.  The smoke test needed the same guard.
     */
    SysTick->CTRL &= ~SysTick_CTRL_TICKINT_Msk;
    SCB->ICSR = SCB_ICSR_PENDSTCLR_Msk;

    vTaskStartScheduler();

    /* Only reached when the idle/timer task could not be created. */
    Error_Handler();
}
