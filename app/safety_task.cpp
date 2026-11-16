#include "safety_task.hpp"

#include "comm_task.hpp"
#include "control_loop.hpp"
#include "imu_source.hpp"
#include "modules/logger/logger.h"

#include <cstdint>

namespace {

constexpr UBaseType_t kSafetyPriority = 4U;
constexpr configSTACK_DEPTH_TYPE kSafetyStackWords = 128U;
constexpr std::uint32_t kSafetyPeriodMs = 10U;
constexpr std::uint32_t kStartupGraceMs = 200U;
constexpr std::uint32_t kImuTimeoutMs = 20U;
constexpr std::uint32_t kControlTimeoutMs = 50U;
constexpr std::uint32_t kCommTimeoutMs = 500U;

TaskHandle_t g_task{nullptr};

void safety_task(void* argument) {
    auto* events = static_cast<EventGroupHandle_t>(argument);
    if (events == nullptr) {
        for (;;) {
            vTaskDelay(portMAX_DELAY);
        }
    }

    TickType_t last_wake = xTaskGetTickCount();
    const TickType_t startup = last_wake;
    std::uint32_t last_imu_seq = imu_source_last_seq();
    std::uint32_t last_control_seq = control_loop_last_seq();
    TickType_t imu_changed = startup;
    TickType_t control_changed = startup;

    for (;;) {
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(kSafetyPeriodMs));

        const TickType_t now = xTaskGetTickCount();

        const std::uint32_t imu_seq = imu_source_last_seq();
        if (imu_seq != last_imu_seq) {
            last_imu_seq = imu_seq;
            imu_changed = now;
        }

        const std::uint32_t control_seq = control_loop_last_seq();
        if (control_seq != last_control_seq) {
            last_control_seq = control_seq;
            control_changed = now;
        }

        EventBits_t new_bits = 0U;
        if ((now - startup) >= pdMS_TO_TICKS(kStartupGraceMs)) {
            if ((now - imu_changed) > pdMS_TO_TICKS(kImuTimeoutMs)) {
                new_bits |= kSafetyImuTimeout;
            }
            if ((now - control_changed) > pdMS_TO_TICKS(kControlTimeoutMs)) {
                new_bits |= kSafetyControlTimeout;
            }
            if (comm_has_seen() &&
                ((now - comm_last_rx_tick()) >
                 pdMS_TO_TICKS(kCommTimeoutMs))) {
                new_bits |= kSafetyCommTimeout;
            }
        }

        const EventBits_t old_bits = xEventGroupGetBits(events);
        const EventBits_t raised = new_bits & ~old_bits;
        if (raised != 0U) {
            (void)xEventGroupSetBits(events, raised);
            LOG_WARN("safety", "fault=0x%02lx imu=%lu ctl=%lu",
                     static_cast<unsigned long>(raised),
                     static_cast<unsigned long>(imu_seq),
                     static_cast<unsigned long>(control_seq));
        }
    }
}

}  // namespace

TaskHandle_t safety_task_start(EventGroupHandle_t events) {
    if (events == nullptr) {
        return nullptr;
    }

    if (xTaskCreate(safety_task, "safety", kSafetyStackWords, events,
                    kSafetyPriority, &g_task) != pdPASS) {
        g_task = nullptr;
        return nullptr;
    }

    return g_task;
}
