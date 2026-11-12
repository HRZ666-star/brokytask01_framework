#include "board.h"
#include "board_uart.h"

#include <cstddef>
#include <cstdio>

namespace {

void send_line(bsp::IUart& uart, const char* text, std::size_t size) {
    const common::Error error = uart.write(
        {reinterpret_cast<const std::uint8_t*>(text), size}, 100U);
    if (error != common::Error::Ok) {
        // The UART is the only console on this board; a failed write is
        // signalled by turning the LED on instead of failing silently.
        board_led_write(1);
    }
}

}  // namespace

int main(void) {
    board_init();
    bsp::IUart& uart = board_uart();

    char line[96];
    int length = std::snprintf(
        line, sizeof(line), "hello from f103, sysclk=%lu\r\n",
        static_cast<unsigned long>(HAL_RCC_GetHCLKFreq()));
    if (length > 0) {
        send_line(uart, line, static_cast<std::size_t>(length));
    }

    for (;;) {
        board_led_toggle();
        HAL_Delay(500);

        length = std::snprintf(line, sizeof(line), "tick %lu\r\n",
                               static_cast<unsigned long>(HAL_GetTick()));
        if (length > 0) {
            send_line(uart, line, static_cast<std::size_t>(length));
        }
    }
}
