#pragma once

#include "FreeRTOS.h"
#include "queue.h"

#include <cstdint>

/*
 * Snapshot handed from the 1 kHz IMU producer to the control task.
 * POD: it is copied through a FreeRTOS queue, no allocation.
 */
struct ImuSnapshot {
    std::uint32_t seq;
    std::uint32_t period_us;
    std::int32_t roll_mdeg;
    std::int32_t pitch_mdeg;
    std::int32_t yaw_mdeg;
};

/*
 * Create the single-slot IMU queue and start the 1 kHz producer task.
 * Returns the queue handle, or nullptr if creation failed.
 */
QueueHandle_t imu_source_start(UBaseType_t priority = 6U,
                               configSTACK_DEPTH_TYPE stack_words = 256U);

/* Latest producer state, used by safety and telemetry without sharing queues. */
std::uint32_t imu_source_last_seq() noexcept;
std::uint32_t imu_source_last_period_us() noexcept;
