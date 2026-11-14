#pragma once

#include <cstddef>
#include <cstdint>

namespace common {

/*
 * Log levels are ordered from most verbose to most severe.  Off is a sentinel
 * used by compile-time filtering; it is never stored in a LogRecord.
 */
enum class LogLevel : std::uint8_t {
    Trace = 0,
    Debug,
    Info,
    Warn,
    Error,
    Fatal,
    Off,
};

constexpr std::size_t kLogTagMax = 16;
constexpr std::size_t kLogTextMax = 96;

/*
 * Fixed-size log record.
 *
 * The record is deliberately POD and copyable: it is copied into a FreeRTOS
 * queue, so it must not own heap memory or contain a non-trivial member.
 */
struct LogRecord {
    std::uint32_t timestamp_ms{0};
    LogLevel level{LogLevel::Info};
    char tag[kLogTagMax]{};
    char text[kLogTextMax]{};
};

const char* to_string(LogLevel level) noexcept;

}  // namespace common
