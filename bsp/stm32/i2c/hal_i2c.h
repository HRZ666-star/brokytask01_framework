#pragma once

#include "bsp/interface/i2c/i2c.h"

#if defined(STM32F1)
#include "stm32f1xx_hal.h"
#elif defined(STM32F4)
#include "stm32f4xx_hal.h"
#else
#error "Define STM32F1 or STM32F4 for the STM32 I2C adapter"
#endif

namespace bsp {

/*
 * STM32 HAL adapter for II2c.  Only this file (and the board layer) may see
 * I2C_HandleTypeDef; callers use II2c and plain spans.
 */
class HalI2c final : public II2c {
public:
    explicit HalI2c(I2C_HandleTypeDef& handle) noexcept;

    [[nodiscard]] Error init(const I2cConfig& config) override;

    [[nodiscard]] Error write(std::uint8_t address,
                              ConstByteSpan data,
                              std::uint32_t timeout_ms) override;
    [[nodiscard]] Error read(std::uint8_t address,
                             ByteSpan out,
                             std::uint32_t timeout_ms) override;

    [[nodiscard]] Error memWrite(std::uint8_t address,
                                 std::uint8_t mem_address,
                                 ConstByteSpan data,
                                 std::uint32_t timeout_ms) override;
    [[nodiscard]] Error memRead(std::uint8_t address,
                                std::uint8_t mem_address,
                                ByteSpan out,
                                std::uint32_t timeout_ms) override;

    [[nodiscard]] Error memWriteAsync(std::uint8_t address,
                                      std::uint8_t mem_address,
                                      ConstByteSpan data,
                                      I2cCallback callback,
                                      void* context) override;
    [[nodiscard]] Error memReadAsync(std::uint8_t address,
                                     std::uint8_t mem_address,
                                     ByteSpan out,
                                     I2cCallback callback,
                                     void* context) override;

    [[nodiscard]] bool busy() const noexcept override;
    [[nodiscard]] I2cMode mode() const noexcept override;

    /* HAL callback bridges. */
    static void on_complete(I2C_HandleTypeDef* handle, Error result);
    static Error map_error(std::uint32_t hal_error) noexcept;

private:
    I2C_HandleTypeDef& handle_;
    I2cMode mode_ = I2cMode::Blocking;
    bool initialized_ = false;
    volatile bool busy_ = false;
    I2cCallback callback_ = nullptr;
    void* context_ = nullptr;

    static HalI2c* active_;
};

}  // namespace bsp
