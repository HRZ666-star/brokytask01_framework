#include "modules/logger/logger.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace modules {
namespace {

void copy_tag(char (&destination)[common::kLogTagMax], const char* source) noexcept {
    if (source == nullptr) {
        destination[0] = '\0';
        return;
    }

    std::size_t index = 0;
    while ((index + 1U) < common::kLogTagMax && source[index] != '\0') {
        destination[index] = source[index];
        ++index;
    }
    destination[index] = '\0';
}

}  // namespace

Logger& logger() noexcept {
    /*
     * Function-local static: one logger per image, no heap allocation.  The
     * first call happens during startup, before any task can race with it.
     */
    static Logger instance;
    return instance;
}

common::Error Logger::init(bsp::IUart& uart) noexcept {
    if (initialized_) {
        return common::Error::Ok;
    }

    uart_ = &uart;
    queue_ = xQueueCreate(kQueueDepth, sizeof(common::LogRecord));
    if (queue_ == nullptr) {
        uart_ = nullptr;
        return common::Error::Full;
    }

    const BaseType_t created = xTaskCreate(
        task_entry, "logger", static_cast<configSTACK_DEPTH_TYPE>(kTaskStackWords),
        this, kTaskPriority, &task_);
    if (created != pdPASS) {
        vQueueDelete(queue_);
        queue_ = nullptr;
        task_ = nullptr;
        uart_ = nullptr;
        return common::Error::Full;
    }

    initialized_ = true;
    return common::Error::Ok;
}

common::Error Logger::log(common::LogLevel level,
                          const char* tag,
                          const char* format,
                          ...) noexcept {
    if (!initialized_ || queue_ == nullptr) {
        return common::Error::NotInitialized;
    }

    common::LogRecord record{};
    record.timestamp_ms = static_cast<std::uint32_t>(xTaskGetTickCount());
    record.level = level;
    copy_tag(record.tag, tag);

    if (format != nullptr) {
        va_list arguments;
        va_start(arguments, format);
        std::vsnprintf(record.text, sizeof(record.text), format, arguments);
        va_end(arguments);
    }

    if (xQueueSend(queue_, &record, 0U) != pdPASS) {
        taskENTER_CRITICAL();
        ++dropped_;
        taskEXIT_CRITICAL();
        return common::Error::Full;
    }

    return common::Error::Ok;
}

void Logger::task_entry(void* context) {
    static_cast<Logger*>(context)->run();
}

void Logger::run() noexcept {
    for (;;) {
        common::LogRecord record{};
        if (xQueueReceive(queue_, &record, portMAX_DELAY) != pdPASS) {
            continue;
        }

        char line[160];
        const int length = std::snprintf(
            line, sizeof(line), "[%lu] [%s] %s: %s\r\n",
            static_cast<unsigned long>(record.timestamp_ms),
            common::to_string(record.level), record.tag, record.text);
        if (length <= 0) {
            continue;
        }

        std::size_t size = static_cast<std::size_t>(length);
        if (size >= sizeof(line)) {
            size = sizeof(line) - 1U;
        }

        const common::Error written =
            uart_->write({reinterpret_cast<const std::uint8_t*>(line), size}, 100U);
        if (written != common::Error::Ok) {
            taskENTER_CRITICAL();
            ++write_errors_;
            taskEXIT_CRITICAL();
        }
    }
}

std::uint32_t Logger::dropped() const noexcept {
    return dropped_;
}

std::uint32_t Logger::write_errors() const noexcept {
    return write_errors_;
}

bool Logger::initialized() const noexcept {
    return initialized_;
}

}  // namespace modules
