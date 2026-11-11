#include "board.h"
#include <cstdio>

int main(void) {
    board_init();

    std::printf("hello from f103, sysclk=%lu\r\n",
                static_cast<unsigned long>(HAL_RCC_GetHCLKFreq()));

    for (;;) {
        board_led_toggle();
        HAL_Delay(500);
        std::printf("tick %lu\r\n",
                    static_cast<unsigned long>(HAL_GetTick()));
    }
}
