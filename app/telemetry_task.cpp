#include "telemetry_task.hpp"

#include "modules/logger/logger.h"

#include <cstdint>

namespace {

constexpr UBaseType_t kTelemetryPriority = 2U;
constexpr configSTACK_DEPTH_TYPE kTelemetryStackWords = 256U;
constexpr std::uint32_t kTelemetryPeriodMs = 50U;

TaskHandle_t g_task{nullptr};

void telemetry_task(void* argument) {
    auto* queue = static_cast<QueueHandle_t>(argument);
    if (queue == nullptr) {
        for (;;) {
            vTaskDelay(portMAX_DELAY);
        }
    }

    TickType_t last_wake = xTaskGetTickCount();

    for (;;) {
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(kTelemetryPeriodMs));

        TelemetrySnapshot snapshot{};
        if (xQueueReceive(queue, &snapshot, 0U) != pdTRUE) {
            continue;
        }

        LOG_INFO("tele",
                 "t=%lu imu=%lu p=%ld tgt=%ld cmd=%ld a=%d rx=%lu err=%lu cerr=%lu drop=%lu f=0x%02lx",
                 static_cast<unsigned long>(snapshot.seq),
                 static_cast<unsigned long>(snapshot.imu_seq),
                 static_cast<long>(snapshot.pitch_mdeg),
                 static_cast<long>(snapshot.target_mdeg),
                 static_cast<long>(snapshot.command),
                 static_cast<int>(snapshot.servo_deg),
                 static_cast<unsigned long>(snapshot.comm_rx),
                 static_cast<unsigned long>(snapshot.comm_errors),
                 static_cast<unsigned long>(snapshot.comm_crc_errors),
                 static_cast<unsigned long>(snapshot.comm_drops),
                 static_cast<unsigned long>(snapshot.faults));
    }
}

}  // namespace

TaskHandle_t telemetry_task_start(QueueHandle_t snapshot_queue,
                                  EventGroupHandle_t events) {
    (void)events;

    if (snapshot_queue == nullptr) {
        return nullptr;
    }

    if (xTaskCreate(telemetry_task, "telemetry", kTelemetryStackWords,
                    snapshot_queue, kTelemetryPriority, &g_task) != pdPASS) {
        g_task = nullptr;
        return nullptr;
    }

    return g_task;
}
