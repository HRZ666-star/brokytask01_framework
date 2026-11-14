#pragma once

#include "bsp/interface/span.h"
#include "common/error/error.h"

#include <cstdint>

namespace bsp {

using common::Error;

enum class UartMode : std::uint8_t {
    Blocking,
    Interrupt,
    Dma,
};

struct UartConfig {
    std::uint32_t baudrate = 115200;
    UartMode mode = UartMode::Blocking;
};

using UartRxCallback = void (*)(void* context, ConstByteSpan data);

/*
 * The only UART type visible above the adapter layer.  Implementations are
 * injected by the board (see board/f103) so application and module code never
 * includes a HAL header.
 */
class IUart {
public:
    virtual ~IUart() = default;

    [[nodiscard]] virtual Error init(const UartConfig& config) = 0;
    [[nodiscard]] virtual Error write(ConstByteSpan data,
                                      std::uint32_t timeout_ms) = 0;
    [[nodiscard]] virtual Error read(ByteSpan out,
                                     std::uint32_t timeout_ms) = 0;
    [[nodiscard]] virtual Error startReceive(UartRxCallback callback,
                                             void* context) = 0;
    [[nodiscard]] virtual Error stopReceive() = 0;
    [[nodiscard]] virtual UartMode mode() const noexcept = 0;
};

}  // namespace bsp
