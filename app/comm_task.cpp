#include "comm_task.hpp"

#include "modules/logger/logger.h"
#include "modules/protocol/frame_parser.h"

#include <cstddef>
#include <cstdint>

namespace {

constexpr UBaseType_t kCommPriority = 4U;
constexpr configSTACK_DEPTH_TYPE kCommStackWords = 384U;

StreamBufferHandle_t g_rx_stream{nullptr};
QueueHandle_t g_command_queue{nullptr};
TaskHandle_t g_task{nullptr};
modules::FrameParser g_parser;

volatile TickType_t g_last_rx_tick{0U};
volatile bool g_seen{false};
volatile std::uint32_t g_rx_bytes{0U};
volatile std::uint32_t g_frame_errors{0U};
volatile std::uint32_t g_command_drops{0U};

void handle_frame(const modules::Frame& frame) {
    g_last_rx_tick = xTaskGetTickCount();
    g_seen = true;

    if (frame.command != 0x01U || frame.length < 4U) {
        ++g_frame_errors;
        return;
    }

    const std::uint32_t raw =
        (static_cast<std::uint32_t>(frame.payload[0]) << 24U) |
        (static_cast<std::uint32_t>(frame.payload[1]) << 16U) |
        (static_cast<std::uint32_t>(frame.payload[2]) << 8U) |
        static_cast<std::uint32_t>(frame.payload[3]);
    const Command command{0x01U, static_cast<std::int32_t>(raw)};

    if ((g_command_queue != nullptr) &&
        (xQueueSend(g_command_queue, &command, 0U) != pdPASS)) {
        ++g_command_drops;
        return;
    }

    LOG_INFO("comm", "cmd=0x%02x target=%ld mdps",
             static_cast<unsigned>(command.id),
             static_cast<long>(command.value));
}

void comm_task(void* argument) {
    (void)argument;

    std::uint8_t byte = 0U;
    modules::Frame frame{};

    for (;;) {
        const std::size_t received =
            xStreamBufferReceive(g_rx_stream, &byte, 1U, portMAX_DELAY);
        if (received != 1U) {
            continue;
        }

        ++g_rx_bytes;
        if (g_parser.feed(byte, frame)) {
            handle_frame(frame);
        }
    }
}

}  // namespace

void comm_uart_rx_isr(void* context, bsp::ConstByteSpan data) {
    auto* stream = static_cast<StreamBufferHandle_t>(context);
    if (stream == nullptr || data.empty()) {
        return;
    }

    BaseType_t higher_priority_task_woken = pdFALSE;
    (void)xStreamBufferSendFromISR(stream, data.data, data.size,
                                   &higher_priority_task_woken);
    portYIELD_FROM_ISR(higher_priority_task_woken);
}

TaskHandle_t comm_task_start(StreamBufferHandle_t rx_stream,
                             QueueHandle_t command_queue,
                             EventGroupHandle_t events) {
    (void)events;

    if (rx_stream == nullptr || command_queue == nullptr) {
        return nullptr;
    }

    g_rx_stream = rx_stream;
    g_command_queue = command_queue;
    g_parser.reset();

    if (xTaskCreate(comm_task, "comm", kCommStackWords, nullptr,
                    kCommPriority, &g_task) != pdPASS) {
        g_task = nullptr;
        return nullptr;
    }

    return g_task;
}

bool comm_has_seen() noexcept {
    return g_seen;
}

TickType_t comm_last_rx_tick() noexcept {
    return g_last_rx_tick;
}

std::uint32_t comm_rx_bytes() noexcept {
    return g_rx_bytes;
}

std::uint32_t comm_frame_errors() noexcept {
    return g_frame_errors;
}

std::uint32_t comm_command_drops() noexcept {
    return g_command_drops;
}
