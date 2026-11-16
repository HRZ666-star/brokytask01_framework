#pragma once

#include "bsp/interface/span.h"
#include "common/error/error.h"

#include <cstdint>

namespace bsp {

using common::Error;

enum class I2cMode : std::uint8_t {
    Blocking,
    Interrupt,
};

struct I2cConfig {
    std::uint32_t clock_hz = 400000;
    I2cMode mode = I2cMode::Blocking;
};

/*
 * Completion callback for interrupt transfers.  It runs in the I2C interrupt
 * context; the caller's buffer must stay alive until the callback fires.
 */
using I2cCallback = void (*)(void* context, Error result);

/*
 * The only I2C type visible above the adapter layer.  Register-oriented
 * transfers use an 8-bit memory address (the MPU6050 case).
 */
class II2c {
public:
    virtual ~II2c() = default;

    [[nodiscard]] virtual Error init(const I2cConfig& config) = 0;

    [[nodiscard]] virtual Error write(std::uint8_t address,
                                      ConstByteSpan data,
                                      std::uint32_t timeout_ms) = 0;
    [[nodiscard]] virtual Error read(std::uint8_t address,
                                     ByteSpan out,
                                     std::uint32_t timeout_ms) = 0;

    [[nodiscard]] virtual Error memWrite(std::uint8_t address,
                                         std::uint8_t mem_address,
                                         ConstByteSpan data,
                                         std::uint32_t timeout_ms) = 0;
    [[nodiscard]] virtual Error memRead(std::uint8_t address,
                                        std::uint8_t mem_address,
                                        ByteSpan out,
                                        std::uint32_t timeout_ms) = 0;

    /* Interrupt variants.  Return Ok means the transfer was started, not done. */
    [[nodiscard]] virtual Error memWriteAsync(std::uint8_t address,
                                              std::uint8_t mem_address,
                                              ConstByteSpan data,
                                              I2cCallback callback,
                                              void* context) = 0;
    [[nodiscard]] virtual Error memReadAsync(std::uint8_t address,
                                             std::uint8_t mem_address,
                                             ByteSpan out,
                                             I2cCallback callback,
                                             void* context) = 0;

    [[nodiscard]] virtual bool busy() const noexcept = 0;
    [[nodiscard]] virtual I2cMode mode() const noexcept = 0;
};

}  // namespace bsp
