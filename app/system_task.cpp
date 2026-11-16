/*
 * Task-level integration image.
 *
 * Six tasks:
 *   imu       1 kHz producer, highest priority
 *   control   500 Hz PD loop, consumes the newest IMU snapshot
 *   comm      UART command parser, event driven
 *   safety    10 ms liveness/fault supervisor
 *   telemetry 20 Hz CSV-style status report
 *   logger    asynchronous UART log sink (created by Logger::init)
 *
 * The IMU source is synthetic until the MPU6050 is soldered on; the task
 * topology and timing do not change when real samples replace it.
 */
#include "comm_task.hpp"
#include "control_loop.hpp"
#include "imu_source.hpp"
#include "safety_task.hpp"
#include "telemetry_task.hpp"

#include "board.h"
#include "board_uart.h"
#include "modules/logger/logger.h"

#include "FreeRTOS.h"
#include "event_groups.h"
#include "queue.h"
#include "stream_buffer.h"
#include "task.h"

#include <cstddef>
#include <cstdint>

namespace {

constexpr std::size_t kRxStreamBytes = 128U;
constexpr UBaseType_t kCommandQueueDepth = 4U;

}  // namespace

int main(void) {
    board_init();
    bsp::IUart& uart = board_uart();

    /*
     * board_init() brings UART1 up in blocking mode so the early boot path
     * can print without an interrupt vector.  The system image uses the
     * interrupt receive path for commands, so switch the same handle to
     * Interrupt mode before the logger and the receiver take ownership.
     */
    if (uart.init({115200U, bsp::UartMode::Interrupt}) != common::Error::Ok) {
        Error_Handler();
    }

    if (modules::logger().init(uart) != common::Error::Ok) {
        Error_Handler();
    }

    QueueHandle_t imu_queue = imu_source_start(6U, 256U);
    QueueHandle_t command_queue =
        xQueueCreate(kCommandQueueDepth, sizeof(Command));
    QueueHandle_t telemetry_queue =
        xQueueCreate(1U, sizeof(TelemetrySnapshot));
    EventGroupHandle_t safety_events = xEventGroupCreate();
    StreamBufferHandle_t rx_stream =
        xStreamBufferCreate(kRxStreamBytes, 1U);

    if (imu_queue == nullptr || command_queue == nullptr ||
        telemetry_queue == nullptr || safety_events == nullptr ||
        rx_stream == nullptr) {
        Error_Handler();
    }

    if (uart.startReceive(comm_uart_rx_isr, rx_stream) != common::Error::Ok) {
        Error_Handler();
    }

    if (control_loop_start(imu_queue, command_queue, safety_events,
                           telemetry_queue) == nullptr) {
        Error_Handler();
    }

    if (comm_task_start(rx_stream, command_queue, safety_events) == nullptr) {
        Error_Handler();
    }

    if (safety_task_start(safety_events) == nullptr) {
        Error_Handler();
    }

    if (telemetry_task_start(telemetry_queue, safety_events) == nullptr) {
        Error_Handler();
    }

    LOG_INFO("boot", "system six-task demo start, imu=1kHz ctl=500Hz");
    LOG_INFO("boot", "free heap=%u bytes",
             static_cast<unsigned>(xPortGetFreeHeapSize()));

    /*
     * board_init() started SysTick for HAL_Delay().  Hand the counter to
     * FreeRTOS only after clearing the pending HAL tick, otherwise the first
     * scheduler tick can run before the task lists exist.
     */
    SysTick->CTRL &= ~SysTick_CTRL_TICKINT_Msk;
    SCB->ICSR = SCB_ICSR_PENDSTCLR_Msk;

    vTaskStartScheduler();

    Error_Handler();
}
