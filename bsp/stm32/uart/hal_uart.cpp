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

HalUart* HalUart::active_ = nullptr;

HalUart::HalUart(UART_HandleTypeDef& handle) noexcept : handle_(handle) {
    if (active_ == nullptr) {
        active_ = this;
    }
}

Error HalUart::init(const UartConfig& config) {
    if (handle_.Instance == nullptr) {
        return Error::InvalidArgument;
    }
    if (config.baudrate == 0U) {
        return Error::InvalidArgument;
    }

    if (receiving_) {
        (void)HAL_UART_AbortReceive_IT(&handle_);
        receiving_ = false;
    }

    handle_.Init.BaudRate = config.baudrate;
    handle_.Init.WordLength = UART_WORDLENGTH_8B;
    handle_.Init.StopBits = UART_STOPBITS_1;
    handle_.Init.Parity = UART_PARITY_NONE;
    handle_.Init.Mode = UART_MODE_TX_RX;
    handle_.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    handle_.Init.OverSampling = UART_OVERSAMPLING_16;

    if (HAL_UART_Init(&handle_) != HAL_OK) {
        initialized_ = false;
        return Error::Hardware;
    }
    mode_ = config.mode;
    initialized_ = true;
    receiving_ = false;
    return Error::Ok;
}

Error HalUart::write(ConstByteSpan data, std::uint32_t timeout_ms) {
    if (!initialized_) {
        return Error::NotInitialized;
    }
    if (mode_ == UartMode::Dma) {
        // DMA transmit needs a caller-owned buffer and a completion callback;
        // do not silently fall back to a blocking transfer.
        return Error::NotSupported;
    }
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
    if (!initialized_) {
        return Error::NotInitialized;
    }
    if (mode_ != UartMode::Blocking) {
        // In interrupt/DMA mode the callback path owns reception.
        return Error::NotSupported;
    }
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
    if (!initialized_) {
        return Error::NotInitialized;
    }
    if (mode_ != UartMode::Interrupt) {
        return Error::NotSupported;
    }
    if (callback == nullptr) {
        return Error::InvalidArgument;
    }
    if (receiving_) {
        return Error::Busy;
    }

    callback_ = callback;
    callback_context_ = context;
    receiving_ = true;
    const HAL_StatusTypeDef status =
        HAL_UART_Receive_IT(&handle_, &rx_byte_, 1);
    if (status != HAL_OK) {
        receiving_ = false;
        callback_ = nullptr;
        callback_context_ = nullptr;
        return map_hal_status(status);
    }
    return Error::Ok;
}

Error HalUart::stopReceive() {
    if (!initialized_) {
        return Error::NotInitialized;
    }
    if (!receiving_) {
        return Error::Ok;
    }

    receiving_ = false;
    callback_ = nullptr;
    callback_context_ = nullptr;
    const HAL_StatusTypeDef status = HAL_UART_AbortReceive_IT(&handle_);
    return map_hal_status(status);
}

UartMode HalUart::mode() const noexcept { return mode_; }

void HalUart::on_rx_complete(UART_HandleTypeDef* handle) {
    HalUart* self = active_;
    if (self == nullptr || &self->handle_ != handle || !self->receiving_) {
        return;
    }

    if (self->callback_ != nullptr) {
        const ConstByteSpan byte{&self->rx_byte_, 1};
        self->callback_(self->callback_context_, byte);
    }

    // The callback may have called stopReceive(); in that case do not re-arm.
    if (self->receiving_) {
        if (HAL_UART_Receive_IT(&self->handle_, &self->rx_byte_, 1) != HAL_OK) {
            self->receiving_ = false;
        }
    }
}

}  // namespace bsp

extern "C" void HAL_UART_RxCpltCallback(UART_HandleTypeDef* huart) {
    bsp::HalUart::on_rx_complete(huart);
}
