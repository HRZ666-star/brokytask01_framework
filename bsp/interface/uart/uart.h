#pragma once

#include "common/error/error.h"

#include <cstddef>
#include <cstdint>

namespace bsp {

using common::Error;

/*
 * Non-owning view over a contiguous byte buffer.  The framework passes spans
 * instead of raw pointers so the length can never be forgotten at a boundary.
 */
template <typename T>
struct Span {
    T* data = nullptr;
    std::size_t size = 0;

    [[nodiscard]] constexpr bool empty() const noexcept { return size == 0; }
};

using ByteSpan = Span<std::uint8_t>;
using ConstByteSpan = Span<const std::uint8_t>;

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
