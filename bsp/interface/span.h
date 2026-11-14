#pragma once

#include <cstddef>
#include <cstdint>

namespace bsp {

/*
 * Non-owning view over a contiguous buffer.  BSP APIs pass spans instead of
 * pointer/length pairs so the length can never be forgotten at the boundary.
 */
template <typename T>
struct Span {
    T* data = nullptr;
    std::size_t size = 0;

    [[nodiscard]] constexpr bool empty() const noexcept { return size == 0; }
};

using ByteSpan = Span<std::uint8_t>;
using ConstByteSpan = Span<const std::uint8_t>;

}  // namespace bsp
