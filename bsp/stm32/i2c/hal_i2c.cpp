#include "bsp/stm32/i2c/hal_i2c.h"

namespace bsp {

HalI2c* HalI2c::active_ = nullptr;

namespace {

Error map_hal_status(HAL_StatusTypeDef status) noexcept {
    switch (status) {
        case HAL_OK:
            return Error::Ok;
        case HAL_BUSY:
            return Error::Busy;
        case HAL_TIMEOUT:
            return Error::Timeout;
        case HAL_ERROR:
        default:
            return Error::Hardware;
    }
}

Error map_hal_error(std::uint32_t error) noexcept {
    if ((error & HAL_I2C_ERROR_TIMEOUT) != 0U) {
        return Error::Timeout;
    }
    if ((error & HAL_I2C_ERROR_AF) != 0U) {
        return Error::Hardware;
    }
    return Error::Hardware;
}

bool valid_address(std::uint8_t address) noexcept { return address <= 0x7FU; }

bool valid_span(ConstByteSpan data) noexcept {
    return data.size <= 0xFFFFU && (data.size == 0U || data.data != nullptr);
}

bool valid_span(ByteSpan data) noexcept {
    return data.size <= 0xFFFFU && (data.size == 0U || data.data != nullptr);
}

}  // namespace

HalI2c::HalI2c(I2C_HandleTypeDef& handle) noexcept : handle_(handle) {
    if (active_ == nullptr) {
        active_ = this;
    }
}

Error HalI2c::init(const I2cConfig& config) {
    if (handle_.Instance == nullptr) {
        return Error::NotInitialized;
    }
    if (config.clock_hz != 100000U && config.clock_hz != 400000U) {
        return Error::NotSupported;
    }

    handle_.Init.ClockSpeed = config.clock_hz;
    handle_.Init.DutyCycle = I2C_DUTYCYCLE_2;
    handle_.Init.OwnAddress1 = 0U;
    handle_.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    handle_.Init.DualAddressMode = DISABLE;
    handle_.Init.OwnAddress2 = 0U;
    handle_.Init.GeneralCallMode = DISABLE;
    handle_.Init.NoStretchMode = DISABLE;

    if (HAL_I2C_Init(&handle_) != HAL_OK) {
        return Error::Hardware;
    }

    mode_ = config.mode;
    initialized_ = true;
    busy_ = false;
    callback_ = nullptr;
    context_ = nullptr;
    return Error::Ok;
}

Error HalI2c::write(std::uint8_t address,
                    ConstByteSpan data,
                    std::uint32_t timeout_ms) {
    if (!initialized_) {
        return Error::NotInitialized;
    }
    if (!valid_address(address) || !valid_span(data)) {
        return Error::InvalidArgument;
    }
    if (busy_) {
        return Error::Busy;
    }
    if (data.size == 0U) {
        return Error::Ok;
    }

    return map_hal_status(HAL_I2C_Master_Transmit(
        &handle_, static_cast<std::uint16_t>(address << 1U),
        const_cast<std::uint8_t*>(data.data), static_cast<std::uint16_t>(data.size),
        timeout_ms));
}

Error HalI2c::read(std::uint8_t address,
                   ByteSpan out,
                   std::uint32_t timeout_ms) {
    if (!initialized_) {
        return Error::NotInitialized;
    }
    if (!valid_address(address) || !valid_span(out)) {
        return Error::InvalidArgument;
    }
    if (busy_) {
        return Error::Busy;
    }
    if (out.size == 0U) {
        return Error::Ok;
    }

    return map_hal_status(HAL_I2C_Master_Receive(
        &handle_, static_cast<std::uint16_t>(address << 1U), out.data,
        static_cast<std::uint16_t>(out.size), timeout_ms));
}

Error HalI2c::memWrite(std::uint8_t address,
                       std::uint8_t mem_address,
                       ConstByteSpan data,
                       std::uint32_t timeout_ms) {
    if (!initialized_) {
        return Error::NotInitialized;
    }
    if (!valid_address(address) || !valid_span(data)) {
        return Error::InvalidArgument;
    }
    if (busy_) {
        return Error::Busy;
    }
    if (data.size == 0U) {
        return Error::Ok;
    }

    return map_hal_status(HAL_I2C_Mem_Write(
        &handle_, static_cast<std::uint16_t>(address << 1U), mem_address,
        I2C_MEMADD_SIZE_8BIT, const_cast<std::uint8_t*>(data.data),
        static_cast<std::uint16_t>(data.size), timeout_ms));
}

Error HalI2c::memRead(std::uint8_t address,
                      std::uint8_t mem_address,
                      ByteSpan out,
                      std::uint32_t timeout_ms) {
    if (!initialized_) {
        return Error::NotInitialized;
    }
    if (!valid_address(address) || !valid_span(out)) {
        return Error::InvalidArgument;
    }
    if (busy_) {
        return Error::Busy;
    }
    if (out.size == 0U) {
        return Error::Ok;
    }

    return map_hal_status(HAL_I2C_Mem_Read(
        &handle_, static_cast<std::uint16_t>(address << 1U), mem_address,
        I2C_MEMADD_SIZE_8BIT, out.data, static_cast<std::uint16_t>(out.size),
        timeout_ms));
}

Error HalI2c::memWriteAsync(std::uint8_t address,
                            std::uint8_t mem_address,
                            ConstByteSpan data,
                            I2cCallback callback,
                            void* context) {
    if (!initialized_) {
        return Error::NotInitialized;
    }
    if (!valid_address(address) || !valid_span(data) || callback == nullptr) {
        return Error::InvalidArgument;
    }
    if (busy_) {
        return Error::Busy;
    }
    if (data.size == 0U) {
        return Error::Ok;
    }

    callback_ = callback;
    context_ = context;
    busy_ = true;
    const HAL_StatusTypeDef status = HAL_I2C_Mem_Write_IT(
        &handle_, static_cast<std::uint16_t>(address << 1U), mem_address,
        I2C_MEMADD_SIZE_8BIT, const_cast<std::uint8_t*>(data.data),
        static_cast<std::uint16_t>(data.size));
    if (status != HAL_OK) {
        busy_ = false;
        callback_ = nullptr;
        context_ = nullptr;
        return map_hal_status(status);
    }
    return Error::Ok;
}

Error HalI2c::memReadAsync(std::uint8_t address,
                           std::uint8_t mem_address,
                           ByteSpan out,
                           I2cCallback callback,
                           void* context) {
    if (!initialized_) {
        return Error::NotInitialized;
    }
    if (!valid_address(address) || !valid_span(out) || callback == nullptr) {
        return Error::InvalidArgument;
    }
    if (busy_) {
        return Error::Busy;
    }
    if (out.size == 0U) {
        return Error::Ok;
    }

    callback_ = callback;
    context_ = context;
    busy_ = true;
    const HAL_StatusTypeDef status = HAL_I2C_Mem_Read_IT(
        &handle_, static_cast<std::uint16_t>(address << 1U), mem_address,
        I2C_MEMADD_SIZE_8BIT, out.data, static_cast<std::uint16_t>(out.size));
    if (status != HAL_OK) {
        busy_ = false;
        callback_ = nullptr;
        context_ = nullptr;
        return map_hal_status(status);
    }
    return Error::Ok;
}

bool HalI2c::busy() const noexcept { return busy_; }

I2cMode HalI2c::mode() const noexcept { return mode_; }

Error HalI2c::map_error(std::uint32_t hal_error) noexcept {
    return map_hal_error(hal_error);
}

void HalI2c::on_complete(I2C_HandleTypeDef* handle, Error result) {
    HalI2c* self = active_;
    if (self == nullptr || &self->handle_ != handle || !self->busy_) {
        return;
    }

    self->busy_ = false;
    I2cCallback callback = self->callback_;
    void* context = self->context_;
    self->callback_ = nullptr;
    self->context_ = nullptr;

    if (callback != nullptr) {
        callback(context, result);
    }
}

}  // namespace bsp

extern "C" void HAL_I2C_MemTxCpltCallback(I2C_HandleTypeDef* hi2c) {
    bsp::HalI2c::on_complete(hi2c, bsp::Error::Ok);
}

extern "C" void HAL_I2C_MemRxCpltCallback(I2C_HandleTypeDef* hi2c) {
    bsp::HalI2c::on_complete(hi2c, bsp::Error::Ok);
}

extern "C" void HAL_I2C_ErrorCallback(I2C_HandleTypeDef* hi2c) {
    bsp::HalI2c::on_complete(hi2c, bsp::HalI2c::map_error(hi2c->ErrorCode));
}
