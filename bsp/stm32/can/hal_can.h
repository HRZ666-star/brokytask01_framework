#pragma once

#include "bsp/interface/can/can.h"
#include "stm32f1xx_hal.h"

namespace bsp {

/*
 * STM32F1 CAN adapter.  Only this file (and the board layer) may see
 * CAN_HandleTypeDef; callers use ICan and plain CanFrame values.
 *
 * Tested modes:
 *   - Loopback: frame stays inside the controller, no transceiver required.
 *   - Normal:   requires a transceiver (TJA1050) and a second node / ACK.
 */
class HalCan final : public ICan {
public:
    explicit HalCan(CAN_HandleTypeDef& handle) noexcept;

    [[nodiscard]] Error init(const CanConfig& config) override;
    [[nodiscard]] Error start() override;
    [[nodiscard]] Error stop() override;
    [[nodiscard]] Error send(const CanFrame& frame,
                             std::uint32_t timeout_ms) override;
    [[nodiscard]] Error receive(CanFrame& frame,
                                std::uint32_t timeout_ms) override;
    [[nodiscard]] Error startReceive(CanRxCallback callback,
                                     void* context) override;
    [[nodiscard]] Error stopReceive() override;
    [[nodiscard]] CanMode mode() const noexcept override;

    /* HAL callback bridge; called from the CAN RX interrupt. */
    static void on_rx_pending(CAN_HandleTypeDef* handle);

private:
    CAN_HandleTypeDef& handle_;
    CanMode mode_ = CanMode::Normal;
    bool initialized_ = false;
    bool started_ = false;
    bool receiving_ = false;
    CanRxCallback callback_ = nullptr;
    void* context_ = nullptr;

    static HalCan* active_;
};

}  // namespace bsp
