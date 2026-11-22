#pragma once

#include "bsp/interface/uart/uart.h"

#include "FreeRTOS.h"
#include "event_groups.h"
#include "queue.h"
#include "stream_buffer.h"
#include "task.h"

#include <cstdint>

/*
 * Command produced by comm_task and consumed by the control loop.
 * id 0x01: set pitch target in millidegrees (int32 big-endian payload).
 */
struct Command {
    std::uint8_t id;
    std::int32_t value;
};

/*
 * UART RX callback.  It runs in interrupt context: it only copies bytes into
 * the stream buffer, never parses or logs.
 */
void comm_uart_rx_isr(void* context, bsp::ConstByteSpan data);

TaskHandle_t comm_task_start(StreamBufferHandle_t rx_stream,
                             QueueHandle_t command_queue,
                             EventGroupHandle_t events);

bool comm_has_seen() noexcept;
TickType_t comm_last_rx_tick() noexcept;
std::uint32_t comm_rx_bytes() noexcept;
std::uint32_t comm_frame_errors() noexcept;
std::uint32_t comm_crc_errors() noexcept;
std::uint32_t comm_command_drops() noexcept;
