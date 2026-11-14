/*
 * 1 kHz IMU task bring-up.
 *
 * This is the first task of the six-task demo.  It deliberately does not touch
 * the MPU6050 yet: the goal is to prove the 1 kHz time base, the queue hand-off
 * and the stack/period measurements before a sensor is soldered on.
 *
 * Expected serial output (115200 8N1):
 *   [0] [INFO] boot: imu task demo start, target=1000Hz
 *   [1000] [INFO] imu: seq=1000 min=999us max=1002us avg=1000us stack=...
 *   [2000] [INFO] imu: seq=2000 ...
 *
 * The log line is emitted once per second from the IMU task and queued to the
 * asynchronous logger, so the UART write itself does not stretch the 1 ms
 * control period.
 */
#include "board.h"
#include "board_uart.h"
#include "modules/logger/logger.h"

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

#include <cstdint>

namespace {

constexpr UBaseType_t kImuPriority = 6U;
constexpr configSTACK_DEPTH_TYPE kImuStackWords = 256U;
constexpr std::uint32_t kPeriodMs = 1U;
constexpr std::uint32_t kStatsWindow = 1000U;

/*
 * Snapshot handed to the next stage.  It is intentionally a POD so it can be
 * copied through a FreeRTOS queue without allocation or a destructor.
 */
struct ImuSnapshot {
    std::uint32_t seq;
    std::uint32_t period_us;
    std::int32_t roll_mdeg;
    std::int32_t pitch_mdeg;
    std::int32_t yaw_mdeg;
};

struct PeriodStats {
    std::uint32_t min_us{0xFFFFFFFFU};
    std::uint32_t max_us{0U};
    std::uint32_t sum_us{0U};
    std::uint32_t count{0U};

    void add(std::uint32_t period_us) noexcept {
        if (period_us < min_us) {
            min_us = period_us;
        }
        if (period_us > max_us) {
            max_us = period_us;
        }
        sum_us += period_us;
        ++count;
    }

    void reset() noexcept {
        min_us = 0xFFFFFFFFU;
        max_us = 0U;
        sum_us = 0U;
        count = 0U;
    }
};

QueueHandle_t g_imu_queue = nullptr;

void imu_task(void* argument) {
    (void)argument;

    /*
     * board_micros() returns the low 16 bits of the free-running TIM2 counter.
     * At 1 MHz that wraps every 65.536 ms, which is longer than the 1 ms period
     * and lets unsigned 16-bit subtraction measure a period without extending
     * the counter.  This also works on parts whose TIM2 is only 16 bits wide.
     */
    TickType_t last_wake = xTaskGetTickCount();
    std::uint16_t last_us = static_cast<std::uint16_t>(board_micros());
    std::uint32_t seq = 0U;
    PeriodStats stats;

    for (;;) {
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(kPeriodMs));

        const std::uint16_t now_us =
            static_cast<std::uint16_t>(board_micros());
        const std::uint32_t period_us =
            static_cast<std::uint16_t>(now_us - last_us);
        last_us = now_us;

        stats.add(period_us);

        ++seq;
        ImuSnapshot snapshot{};
        snapshot.seq = seq;
        snapshot.period_us = period_us;

        /*
         * Synthetic motion until the MPU6050 is soldered.  Keep the values in
         * millidegrees so the task does not depend on floating point printf.
         */
        snapshot.roll_mdeg =
            static_cast<std::int32_t>((seq % 3600U) * 100U) - 180000;
        snapshot.pitch_mdeg =
            static_cast<std::int32_t>(((seq * 7U) % 3600U) * 100U) - 180000;
        snapshot.yaw_mdeg =
            static_cast<std::int32_t>(((seq * 13U) % 3600U) * 100U) - 180000;

        /* Single-slot queue: the consumer always sees the newest snapshot. */
        (void)xQueueOverwrite(g_imu_queue, &snapshot);

        if (stats.count >= kStatsWindow) {
            const std::uint32_t average_us = stats.sum_us / stats.count;
            LOG_INFO("imu",
                     "seq=%lu min=%luus max=%luus avg=%luus stack=%u",
                     static_cast<unsigned long>(seq),
                     static_cast<unsigned long>(stats.min_us),
                     static_cast<unsigned long>(stats.max_us),
                     static_cast<unsigned long>(average_us),
                     static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)));
            stats.reset();
        }
    }
}

}  // namespace

int main(void) {
    board_init();
    bsp::IUart& uart = board_uart();

    if (modules::logger().init(uart) != common::Error::Ok) {
        Error_Handler();
    }

    g_imu_queue = xQueueCreate(1U, sizeof(ImuSnapshot));
    if (g_imu_queue == nullptr) {
        Error_Handler();
    }

    if (xTaskCreate(imu_task, "imu", kImuStackWords, nullptr, kImuPriority,
                    nullptr) != pdPASS) {
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
