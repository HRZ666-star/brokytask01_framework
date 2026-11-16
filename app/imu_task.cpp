/*
 * Standalone 1 kHz IMU producer image.
 *
 * Usage:
 *   cmake -S . -B build-imu-task -G Ninja \
 *     -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-arm-none-eabi.cmake \
 *     -DBOARD=f103 -DAPP=imu_task
 *   cmake --build build-imu-task
 */
#include "imu_source.hpp"

#include "board.h"
#include "board_uart.h"
#include "modules/logger/logger.h"

#include "FreeRTOS.h"
#include "task.h"

int main(void) {
    board_init();
    bsp::IUart& uart = board_uart();

    if (modules::logger().init(uart) != common::Error::Ok) {
        Error_Handler();
    }

    if (imu_source_start() == nullptr) {
        Error_Handler();
    }

    LOG_INFO("boot", "imu task demo start, target=1000Hz");

    /*
     * board_init() enabled SysTick for HAL_Delay().  Stop its interrupt and
     * clear any pending tick before handing the counter to FreeRTOS.
     */
    SysTick->CTRL &= ~SysTick_CTRL_TICKINT_Msk;
    SCB->ICSR = SCB_ICSR_PENDSTCLR_Msk;

    vTaskStartScheduler();

    /* Only reached when the idle/timer task could not be created. */
    Error_Handler();
}
