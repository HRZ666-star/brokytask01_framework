#pragma once

#include "FreeRTOS.h"
#include "event_groups.h"
#include "task.h"

#include <cstdint>

/*
 * Latched safety fault bits.  The control loop reads the group every cycle and
 * forces a neutral output when any critical bit is set.  Bits are never
 * cleared automatically: a real fault must be investigated and the board
 * reset.
 */
constexpr EventBits_t kSafetyImuTimeout = (1U << 0U);
constexpr EventBits_t kSafetyControlTimeout = (1U << 1U);
constexpr EventBits_t kSafetyCommTimeout = (1U << 2U);
constexpr EventBits_t kSafetyCritical =
    kSafetyImuTimeout | kSafetyControlTimeout;

TaskHandle_t safety_task_start(EventGroupHandle_t events);
