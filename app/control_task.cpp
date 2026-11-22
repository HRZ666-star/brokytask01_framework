/*
 * Standalone 500 Hz control-loop demo.
 *
 * The reusable loop itself lives in control_loop.cpp; this file only wires a
 * synthetic IMU source to it, so the same loop can also run inside the full
 * six-task system image.
 */
#include "control_loop.hpp"
#include "imu_source.hpp"

#include "board.h"
#include "board_uart.h"
#include "modules/logger/logger.h"

#include "FreeRTOS.h"
#include "task.h"

int main(void) {
    BOARD_INIT();
    bsp::IUart& uart = board_uart();

    if (modules::logger().init(uart) != common::Error::Ok) {
        Error_Handler();
    }

    QueueHandle_t imu_queue = imu_source_start(6U, 256U);
    if (imu_queue == nullptr) {
        Error_Handler();
    }

    if (control_loop_start(imu_queue, nullptr, nullptr, nullptr) == nullptr) {
        Error_Handler();
    }

    LOG_INFO("boot", "control task demo start, target=500Hz");

    SysTick->CTRL &= ~SysTick_CTRL_TICKINT_Msk;
    SCB->ICSR = SCB_ICSR_PENDSTCLR_Msk;

    vTaskStartScheduler();

    Error_Handler();
}
