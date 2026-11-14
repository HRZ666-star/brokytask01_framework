#include "board.h"
#include "board_can.h"

#include <cstdio>

namespace {

volatile bool g_irq_seen = false;
bsp::CanFrame g_irq_frame = {};

void on_can_rx(void*, const bsp::CanFrame& frame) {
    g_irq_frame = frame;
    __DMB();
    g_irq_seen = true;
}

void make_frame(bsp::CanFrame& frame, std::uint32_t id, std::uint8_t seed) {
    frame = {};
    frame.id = id;
    frame.dlc = 8U;
    for (std::uint32_t i = 0U; i < 8U; ++i) {
        frame.data[i] = static_cast<std::uint8_t>(seed + i);
    }
}

void print_frame(const char* tag, const bsp::CanFrame& frame) {
    std::printf("%s: id=0x%03lX dlc=%u data=", tag,
                static_cast<unsigned long>(frame.id),
                static_cast<unsigned>(frame.dlc));
    for (std::uint32_t i = 0U; i < frame.dlc; ++i) {
        std::printf("%02X", static_cast<unsigned>(frame.data[i]));
    }
    std::printf("\r\n");
}

}  // namespace

int main() {
    board_init();

    bsp::ICan& can = board_can();
    if (can.init({500000U, bsp::CanMode::Loopback}) != common::Error::Ok) {
        std::printf("can test: init failed\r\n");
        Error_Handler();
    }
    if (can.start() != common::Error::Ok) {
        std::printf("can test: start failed\r\n");
        Error_Handler();
    }
    std::printf("can test: loopback 500 kbit/s\r\n");

    bsp::CanFrame tx = {};
    make_frame(tx, 0x123U, 0x10U);
    if (can.send(tx, 100U) != common::Error::Ok) {
        std::printf("can test: poll send failed\r\n");
        Error_Handler();
    }

    bsp::CanFrame rx = {};
    const common::Error poll_error = can.receive(rx, 100U);
    if (poll_error == common::Error::Ok) {
        print_frame("can poll rx", rx);
    } else {
        std::printf("can poll rx failed: %s\r\n",
                    common::to_string(poll_error));
    }

    if (can.startReceive(on_can_rx, nullptr) != common::Error::Ok) {
        std::printf("can test: startReceive failed\r\n");
        Error_Handler();
    }
    make_frame(tx, 0x124U, 0x20U);
    if (can.send(tx, 100U) != common::Error::Ok) {
        std::printf("can test: irq send failed\r\n");
        Error_Handler();
    }
    for (std::uint32_t i = 0U; i < 100U && !g_irq_seen; ++i) {
        HAL_Delay(1U);
    }
    __DMB();
    if (g_irq_seen) {
        print_frame("can irq  rx", g_irq_frame);
    } else {
        std::printf("can irq  rx: timeout\r\n");
    }
    (void)can.stopReceive();

    std::uint32_t counter = 0U;
    for (;;) {
        make_frame(tx, 0x200U + (counter & 0x0FU),
                   static_cast<std::uint8_t>(counter));
        tx.data[7] = static_cast<std::uint8_t>(counter);

        if (can.send(tx, 100U) == common::Error::Ok &&
            can.receive(rx, 100U) == common::Error::Ok) {
            print_frame("can poll", rx);
        } else {
            std::printf("can poll: send/receive error\r\n");
        }
        ++counter;
        HAL_Delay(1000U);
    }
}
