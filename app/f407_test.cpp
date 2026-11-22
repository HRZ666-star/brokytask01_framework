#include "board.h"
#include <cstdio>

int main() {
    BOARD_INIT();

    std::printf("f407 port: boot, sysclk=%lu\r\n",
                static_cast<unsigned long>(SystemCoreClock));

    uint32_t last = 0;
    for (;;) {
        const uint32_t now = board_timer_ticks();
        if ((now - last) >= 1000U) {
            last = now;
            std::printf("f407 timer: %lu\r\n", static_cast<unsigned long>(now));
            board_led_toggle();
        }
    }
}
