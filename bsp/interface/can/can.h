#pragma once

#include "common/error/error.h"

#include <cstdint>

namespace bsp {

using common::Error;

/*
 * CAN operating mode.  Loopback keeps the frame inside the controller, so the
 * driver can be tested before a transceiver or a second node is available.
 */
enum class CanMode : std::uint8_t {
    Normal,
    Loopback,
};

/*
 * One classic CAN frame (CAN 2.0A/2.0B).  The structure is deliberately plain
 * data: it crosses the BSP boundary and must not expose HAL types.
 */
struct CanFrame {
    std::uint32_t id = 0;
    bool extended = false;
    bool remote = false;
    std::uint8_t dlc = 0;
    std::uint8_t data[8] = {};
};

struct CanConfig {
    std::uint32_t bitrate = 500000;
    CanMode mode = CanMode::Normal;
};

using CanRxCallback = void (*)(void* context, const CanFrame& frame);

/*
 * The only CAN type visible above the adapter layer.  The board injects a
 * concrete implementation; application and module code never includes a HAL
 * header.
 */
class ICan {
public:
    virtual ~ICan() = default;

    [[nodiscard]] virtual Error init(const CanConfig& config) = 0;
    [[nodiscard]] virtual Error start() = 0;
    [[nodiscard]] virtual Error stop() = 0;

    /* Blocking transmit: waits for a free mailbox up to timeout_ms. */
    [[nodiscard]] virtual Error send(const CanFrame& frame,
                                     std::uint32_t timeout_ms) = 0;

    /* Polling receive: timeout_ms == 0 means "do not wait". */
    [[nodiscard]] virtual Error receive(CanFrame& frame,
                                        std::uint32_t timeout_ms) = 0;

    /* Interrupt receive: callback runs in the CAN RX interrupt context. */
    [[nodiscard]] virtual Error startReceive(CanRxCallback callback,
                                             void* context) = 0;
    [[nodiscard]] virtual Error stopReceive() = 0;

    [[nodiscard]] virtual CanMode mode() const noexcept = 0;
};

}  // namespace bsp
