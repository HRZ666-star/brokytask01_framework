#include "board.h"
#include "board_uart.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>

namespace {

volatile std::uint32_t g_rx_count = 0;
volatile std::uint8_t g_last_byte = 0;

void on_rx_byte(void*, bsp::ConstByteSpan data) {
    if (!data.empty()) {
        g_last_byte = data.data[0];
        g_rx_count = g_rx_count + 1U;
    }
}

void send_line(bsp::IUart& uart, const char* text, std::size_t size) {
    if (uart.write({reinterpret_cast<const std::uint8_t*>(text), size},
                   100U) != common::Error::Ok) {
        board_led_write(1);
    }
}

}  // namespace

int main(void) {
    board_init();
    bsp::IUart& uart = board_uart();

    if (uart.init({115200U, bsp::UartMode::Interrupt}) != common::Error::Ok) {
        Error_Handler();
    }
    if (uart.startReceive(on_rx_byte, nullptr) != common::Error::Ok) {
        Error_Handler();
    }

    const char banner[] = "uart test: interrupt rx ready\r\n";
    send_line(uart, banner, sizeof(banner) - 1U);

    char line[96];
    for (;;) {
        const int length = std::snprintf(
            line, sizeof(line), "rx count=%lu last=0x%02X\r\n",
            static_cast<unsigned long>(g_rx_count),
            static_cast<unsigned>(g_last_byte));
        if (length > 0) {
            send_line(uart, line, static_cast<std::size_t>(length));
        }
        HAL_Delay(1000);
    }
}
