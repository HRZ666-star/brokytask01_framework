#pragma once

#include "FreeRTOS.h"
#include "event_groups.h"
#include "queue.h"
#include "task.h"

#include <cstdint>

/*
 * Latest-value snapshot for the 20 Hz telemetry task.  The control loop uses
 * xQueueOverwrite, so a slow serial link can never block the 500 Hz loop.
 */
struct TelemetrySnapshot {
    std::uint32_t seq;
    std::uint32_t imu_seq;
    std::uint32_t comm_rx;
    std::uint32_t comm_errors;
    std::uint32_t comm_crc_errors;
    std::uint32_t comm_drops;
    std::uint32_t faults;
    std::int32_t pitch_mdeg;
    std::int32_t target_mdeg;
    std::int32_t command;
    std::int16_t servo_deg;
};

TaskHandle_t telemetry_task_start(QueueHandle_t snapshot_queue,
                                  EventGroupHandle_t events);
