#pragma once

#include "FreeRTOS.h"
#include "event_groups.h"
#include "queue.h"
#include "task.h"

#include <cstdint>

/*
 * Create the 500 Hz control task.
 *
 * imu_queue       single-slot ImuSnapshot queue (required)
 * command_queue   Command queue from comm_task (optional)
 * safety_events   latched fault bits from safety_task (optional)
 * telemetry_queue single-slot TelemetrySnapshot queue (optional)
 *
 * Returns the control task handle, or nullptr if PWM/servo/task init failed.
 */
TaskHandle_t control_loop_start(QueueHandle_t imu_queue,
                                QueueHandle_t command_queue,
                                EventGroupHandle_t safety_events,
                                QueueHandle_t telemetry_queue);

/* Monotonic sequence, used by safety_task to prove the loop is alive. */
std::uint32_t control_loop_last_seq() noexcept;
