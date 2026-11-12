#include "bsp/stm32/uart/hal_uart.h"

namespace bsp {
namespace {

Error map_hal_status(HAL_StatusTypeDef status) noexcept {
    switch (status) {
        case HAL_OK:
            return Error::Ok;
        case HAL_TIMEOUT:
            return Error::Timeout;
        case HAL_BUSY:
            return Error::Busy;
        case HAL_ERROR:
        default:
            return Error::Hardware;
    }
}

}  // namespace

HalUart::HalUart(UART_HandleTypeDef& handle) noexcept : handle_(handle) {}

Error HalUart::init(const UartConfig& config) {
    if (handle_.Instance == nullptr) {
        return Error::InvalidArgument;
    }
    if (config.baudrate == 0U) {
        return Error::InvalidArgument;
    }

    handle_.Init.BaudRate = config.baudrate;
    handle_.Init.WordLength = UART_WORDLENGTH_8B;
    handle_.Init.StopBits = UART_STOPBITS_1;
    handle_.Init.Parity = UART_PARITY_NONE;
    handle_.Init.Mode = UART_MODE_TX_RX;
    handle_.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    handle_.Init.OverSampling = UART_OVERSAMPLING_16;

    if (HAL_UART_Init(&handle_) != HAL_OK) {
        return Error::Hardware;
    }
    mode_ = config.mode;
    return Error::Ok;
}

Error HalUart::write(ConstByteSpan data, std::uint32_t timeout_ms) {
    if (data.data == nullptr && data.size != 0U) {
        return Error::InvalidArgument;
    }
    if (data.size > 0xFFFFU) {
        return Error::InvalidArgument;
    }
    if (data.size == 0U) {
        return Error::Ok;
    }

    const HAL_StatusTypeDef status =
        HAL_UART_Transmit(&handle_, const_cast<std::uint8_t*>(data.data),
                          static_cast<std::uint16_t>(data.size), timeout_ms);
    return map_hal_status(status);
}

Error HalUart::read(ByteSpan out, std::uint32_t timeout_ms) {
    if (out.data == nullptr && out.size != 0U) {
        return Error::InvalidArgument;
    }
    if (out.size > 0xFFFFU) {
        return Error::InvalidArgument;
    }
    if (out.size == 0U) {
        return Error::Ok;
    }

    const HAL_StatusTypeDef status =
        HAL_UART_Receive(&handle_, out.data,
                         static_cast<std::uint16_t>(out.size), timeout_ms);
    return map_hal_status(status);
}

Error HalUart::startReceive(UartRxCallback callback, void* context) {
    (void)callback;
    (void)context;
    return Error::NotSupported;
}

Error HalUart::stopReceive() { return Error::NotSupported; }

UartMode HalUart::mode() const noexcept { return mode_; }

}  // namespace bsp
