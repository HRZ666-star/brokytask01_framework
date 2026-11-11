#include "board.h"

int main(void) {
    board_init();

    for (;;) {
        board_led_toggle();
        HAL_Delay(500);
    }
}
