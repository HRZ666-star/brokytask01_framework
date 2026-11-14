#pragma once

#include "bsp/interface/uart/uart.h"
#include "common/log/log.h"

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

#include <cstddef>
#include <cstdint>

namespace modules {

/*
 * Asynchronous, fixed-capacity logger.
 *
 * Tasks call log()/LOG_* which formats one bounded record and posts it to a
 * queue.  A lowest-priority task drains the queue and writes to the injected
 * IUart.  A full queue drops the new record and increments dropped(); callers
 * are never blocked and an ISR is never allowed to format a string here.
 */
class Logger final {
public:
    static constexpr std::size_t kQueueDepth = 16U;
    static constexpr std::uint32_t kTaskStackWords = 512U;
    static constexpr UBaseType_t kTaskPriority = 1U;

    [[nodiscard]] common::Error init(bsp::IUart& uart) noexcept;

    /*
     * Not [[nodiscard]]: logging is best-effort by design.  The error code is
     * still returned so a caller that cares can detect NotInitialized/Full.
     */
    common::Error log(common::LogLevel level,
                      const char* tag,
                      const char* format,
                      ...) noexcept;

    [[nodiscard]] std::uint32_t dropped() const noexcept;
    [[nodiscard]] std::uint32_t write_errors() const noexcept;
    [[nodiscard]] bool initialized() const noexcept;

private:
    friend Logger& logger() noexcept;
    Logger() = default;

    static void task_entry(void* context);
    void run() noexcept;

    QueueHandle_t queue_{nullptr};
    TaskHandle_t task_{nullptr};
    bsp::IUart* uart_{nullptr};
    bool initialized_{false};
    volatile std::uint32_t dropped_{0};
    volatile std::uint32_t write_errors_{0};
};

Logger& logger() noexcept;

}  // namespace modules

#define LOG_TRACE(tag, ...) \
    (void)::modules::logger().log(::common::LogLevel::Trace, tag, __VA_ARGS__)
#define LOG_DEBUG(tag, ...) \
    (void)::modules::logger().log(::common::LogLevel::Debug, tag, __VA_ARGS__)
#define LOG_INFO(tag, ...) \
    (void)::modules::logger().log(::common::LogLevel::Info, tag, __VA_ARGS__)
#define LOG_WARN(tag, ...) \
    (void)::modules::logger().log(::common::LogLevel::Warn, tag, __VA_ARGS__)
#define LOG_ERROR(tag, ...) \
    (void)::modules::logger().log(::common::LogLevel::Error, tag, __VA_ARGS__)
#define LOG_FATAL(tag, ...) \
    (void)::modules::logger().log(::common::LogLevel::Fatal, tag, __VA_ARGS__)
