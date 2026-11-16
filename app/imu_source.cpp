/*
 * 1 kHz synthetic IMU producer for the task-pipeline demo.
 *
 * It deliberately does not touch the MPU6050 yet: the goal is to prove the
 * 1 kHz time base, the queue hand-off and the stack/period measurements before
 * the sensor is soldered on.  The control task consumes the same queue.
 */
#include "imu_source.hpp"

#include "board.h"
#include "modules/logger/logger.h"

#include "FreeRTOS.h"
#include "task.h"

#include <cstdint>

namespace {

constexpr std::uint32_t kPeriodMs = 1U;
constexpr std::uint32_t kStatsWindow = 1000U;

QueueHandle_t g_imu_queue = nullptr;
volatile std::uint32_t g_imu_seq = 0U;
volatile std::uint32_t g_imu_period_us = 0U;

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

void imu_task(void* argument) {
    (void)argument;

    /*
     * board_micros() returns the low 16 bits of the free-running TIM2 counter.
     * At 1 MHz that wraps every 65.536 ms; unsigned 16-bit subtraction gives a
     * valid delta for a 1 ms period without extending the counter.
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
        g_imu_seq = seq;
        g_imu_period_us = period_us;
        ImuSnapshot snapshot{};
        snapshot.seq = seq;
        snapshot.period_us = period_us;

        /* Synthetic motion until the MPU6050 is soldered on. */
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

QueueHandle_t imu_source_start(UBaseType_t priority,
                               configSTACK_DEPTH_TYPE stack_words) {
    g_imu_queue = xQueueCreate(1U, sizeof(ImuSnapshot));
    if (g_imu_queue == nullptr) {
        return nullptr;
    }

    if (xTaskCreate(imu_task, "imu", stack_words, nullptr, priority,
                    nullptr) != pdPASS) {
        vQueueDelete(g_imu_queue);
        g_imu_queue = nullptr;
        return nullptr;
    }

    return g_imu_queue;
}

std::uint32_t imu_source_last_seq() noexcept {
    return g_imu_seq;
}

std::uint32_t imu_source_last_period_us() noexcept {
    return g_imu_period_us;
}
