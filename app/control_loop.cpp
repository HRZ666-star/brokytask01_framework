#include "control_loop.hpp"

#include "comm_task.hpp"
#include "imu_source.hpp"
#include "safety_task.hpp"
#include "telemetry_task.hpp"

#include "board.h"
#include "board_pwm.h"
#include "modules/logger/logger.h"
#include "modules/servo/servo.h"

#include <cstdint>

namespace {

constexpr UBaseType_t kControlPriority = 5U;
constexpr configSTACK_DEPTH_TYPE kControlStackWords = 384U;
constexpr std::uint32_t kControlPeriodMs = 2U;  // 500 Hz
constexpr std::uint32_t kStatsWindow = 500U;
constexpr std::uint32_t kTelemetryDivider = 25U;  // 500 Hz / 25 = 20 Hz
constexpr std::int32_t kCommandLimit = 1000;
constexpr std::int32_t kTargetLimitMdeg = 30000;

struct ControlContext {
    QueueHandle_t imu_queue{nullptr};
    QueueHandle_t command_queue{nullptr};
    EventGroupHandle_t safety_events{nullptr};
    QueueHandle_t telemetry_queue{nullptr};
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

std::int32_t clamp_value(std::int64_t value, std::int32_t limit) noexcept {
    if (value > limit) {
        return limit;
    }
    if (value < -limit) {
        return -limit;
    }
    return static_cast<std::int32_t>(value);
}

ControlContext g_context;
modules::Servo g_servo;
TaskHandle_t g_task{nullptr};
volatile std::uint32_t g_sequence{0U};

void control_task(void* argument) {
    auto* context = static_cast<ControlContext*>(argument);
    if (context == nullptr) {
        for (;;) {
            vTaskDelay(portMAX_DELAY);
        }
    }

    TickType_t last_wake = xTaskGetTickCount();
    std::uint16_t last_us = static_cast<std::uint16_t>(board_micros());
    PeriodStats stats;

    ImuSnapshot snapshot{};
    bool have_snapshot = false;
    std::int32_t previous_error_mdeg = 0;
    std::int32_t target_mdeg = 0;
    std::int32_t command = 0;
    std::int16_t servo_angle = 0;
    std::uint32_t sequence = 0U;
    std::uint32_t missed = 0U;
    std::uint32_t saturated = 0U;
    std::uint32_t servo_errors = 0U;

    for (;;) {
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(kControlPeriodMs));

        const std::uint16_t now_us =
            static_cast<std::uint16_t>(board_micros());
        const std::uint32_t period_us =
            static_cast<std::uint16_t>(now_us - last_us);
        last_us = now_us;
        stats.add(period_us);
        ++sequence;
        g_sequence = sequence;

        Command incoming{};
        while ((context->command_queue != nullptr) &&
               (xQueueReceive(context->command_queue, &incoming, 0U) ==
                pdTRUE)) {
            if (incoming.id == 0x01U) {
                target_mdeg = clamp_value(incoming.value, kTargetLimitMdeg);
            }
        }

        ImuSnapshot fresh{};
        if (xQueueReceive(context->imu_queue, &fresh, 0U) == pdTRUE) {
            snapshot = fresh;
            have_snapshot = true;
        } else {
            ++missed;
        }

        const EventBits_t faults =
            (context->safety_events != nullptr)
                ? xEventGroupGetBits(context->safety_events)
                : static_cast<EventBits_t>(0U);

        /*
         * Any critical fault must stop the output.  Comm loss is included:
         * after the operator link disappears the gimbal must not keep
         * executing the last commanded target.
         */
        const bool faulted = (faults & kSafetyCritical) != 0U;

        if (faulted) {
            target_mdeg = 0;
            command = 0;
            servo_angle = 0;
            previous_error_mdeg = 0;
        } else if (have_snapshot) {
            /*
             * Single-loop PD around the commanded pitch.  Units are
             * millidegrees throughout; the derivative uses the fixed 2 ms
             * period and is scaled into the same command domain.
             */
            const std::int32_t error_mdeg =
                target_mdeg - snapshot.pitch_mdeg;
            const std::int32_t derivative_mdeg_s =
                (error_mdeg - previous_error_mdeg) * 500;
            previous_error_mdeg = error_mdeg;

            const std::int64_t raw =
                (1LL * error_mdeg) / 200LL +
                (1LL * derivative_mdeg_s) / 20000LL;
            const std::int32_t limited = clamp_value(raw, kCommandLimit);
            if (limited != raw) {
                ++saturated;
            }
            command = limited;
            servo_angle = static_cast<std::int16_t>(command * 90 / 1000);
        } else {
            command = 0;
            servo_angle = 0;
        }

        if (g_servo.setAngleDeg(servo_angle) != common::Error::Ok) {
            ++servo_errors;
        }

        if ((context->telemetry_queue != nullptr) &&
            ((sequence % kTelemetryDivider) == 0U)) {
            TelemetrySnapshot telemetry{};
            telemetry.seq = sequence;
            telemetry.imu_seq = snapshot.seq;
            telemetry.comm_rx = comm_rx_bytes();
            telemetry.comm_errors = comm_frame_errors();
            telemetry.comm_crc_errors = comm_crc_errors();
            telemetry.comm_drops = comm_command_drops();
            telemetry.faults = static_cast<std::uint32_t>(faults);
            telemetry.pitch_mdeg = snapshot.pitch_mdeg;
            telemetry.target_mdeg = target_mdeg;
            telemetry.command = command;
            telemetry.servo_deg = servo_angle;
            (void)xQueueOverwrite(context->telemetry_queue, &telemetry);
        }

        if (stats.count >= kStatsWindow) {
            const std::uint32_t average_us = stats.sum_us / stats.count;
            LOG_INFO("control",
                     "seq=%lu imu=%lu dt=%luus p=%ld tgt=%ld cmd=%ld ang=%d sat=%lu miss=%lu serr=%lu stack=%u",
                     static_cast<unsigned long>(sequence),
                     static_cast<unsigned long>(snapshot.seq),
                     static_cast<unsigned long>(average_us),
                     static_cast<long>(snapshot.pitch_mdeg),
                     static_cast<long>(target_mdeg),
                     static_cast<long>(command),
                     static_cast<int>(servo_angle),
                     static_cast<unsigned long>(saturated),
                     static_cast<unsigned long>(missed),
                     static_cast<unsigned long>(servo_errors),
                     static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)));
            stats.reset();
            missed = 0U;
            saturated = 0U;
            servo_errors = 0U;
        }
    }
}

}  // namespace

TaskHandle_t control_loop_start(QueueHandle_t imu_queue,
                                QueueHandle_t command_queue,
                                EventGroupHandle_t safety_events,
                                QueueHandle_t telemetry_queue) {
    if (imu_queue == nullptr) {
        return nullptr;
    }

    if (board_pwm().init({50U, 500U, 2500U, 72000000U}) !=
        common::Error::Ok) {
        return nullptr;
    }
    if (g_servo.init(board_pwm(), {500U, 2500U, -90, 90}) !=
        common::Error::Ok) {
        return nullptr;
    }

    g_context.imu_queue = imu_queue;
    g_context.command_queue = command_queue;
    g_context.safety_events = safety_events;
    g_context.telemetry_queue = telemetry_queue;

    if (xTaskCreate(control_task, "control", kControlStackWords, &g_context,
                    kControlPriority, &g_task) != pdPASS) {
        g_task = nullptr;
        return nullptr;
    }

    return g_task;
}

std::uint32_t control_loop_last_seq() noexcept {
    return g_sequence;
}
