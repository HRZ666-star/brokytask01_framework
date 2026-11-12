#pragma once

#include "bsp/interface/uart/uart.h"

#if defined(STM32F1)
#include "stm32f1xx_hal.h"
#elif defined(STM32F4)
#include "stm32f4xx_hal.h"
#else
#error "Define STM32F1 or STM32F4 for the STM32 UART adapter"
#endif

namespace bsp {

/*
 * STM32 HAL adapter for IUart.
 *
 * This is the only UART file allowed to include a HAL header.  The board
 * constructs it with an already configured UART_HandleTypeDef; callers above
 * this layer only see IUart.
 */
class HalUart final : public IUart {
public:
    explicit HalUart(UART_HandleTypeDef& handle) noexcept;

    [[nodiscard]] Error init(const UartConfig& config) override;
    [[nodiscard]] Error write(ConstByteSpan data,
                              std::uint32_t timeout_ms) override;
    [[nodiscard]] Error read(ByteSpan out,
                             std::uint32_t timeout_ms) override;
    [[nodiscard]] Error startReceive(UartRxCallback callback,
                                     void* context) override;
    [[nodiscard]] Error stopReceive() override;
    [[nodiscard]] UartMode mode() const noexcept override;

    /* Internal hook used by HAL_UART_RxCpltCallback(). */
    static void on_rx_complete(UART_HandleTypeDef* handle);

private:
    UART_HandleTypeDef& handle_;
    UartMode mode_{UartMode::Blocking};
    UartRxCallback callback_{nullptr};
    void* callback_context_{nullptr};
    std::uint8_t rx_byte_{0};
    bool initialized_{false};
    bool receiving_{false};
    static HalUart* active_;
};

}  // namespace bsp
