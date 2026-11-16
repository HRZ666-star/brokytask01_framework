#pragma once

#include <cstddef>
#include <cstdint>

namespace modules {

/*
 * Tiny command frame parser for the demo protocol:
 *
 *   0xAA 0x55 | len | cmd | payload[len] | crc8
 *
 * len is the payload length (0..kMaxPayload).  crc8 covers
 * len, cmd and payload, using polynomial 0x07 with an initial value of 0.
 *
 * The parser is byte-oriented so it can run in a normal task, not in an ISR.
 * It never allocates and never blocks.
 */
struct Frame {
    std::uint8_t command{0};
    std::uint8_t length{0};
    std::uint8_t payload[32]{};
};

class FrameParser final {
public:
    static constexpr std::size_t kMaxPayload = 32U;

    /*
     * Feed one byte.  Returns true when a complete, CRC-valid frame has been
     * produced in `out`.  Invalid frames are discarded and the parser
     * resynchronises on the next 0xAA 0x55 sequence.
     */
    bool feed(std::uint8_t byte, Frame& out) noexcept;

    void reset() noexcept;

private:
    enum class State : std::uint8_t {
        WaitA,
        Wait55,
        Length,
        Command,
        Payload,
        Crc,
    };

    static std::uint8_t crc8_update(std::uint8_t crc, std::uint8_t byte) noexcept;

    State state_{State::WaitA};
    std::uint8_t length_{0};
    std::uint8_t index_{0};
    std::uint8_t command_{0};
    std::uint8_t crc_{0};
    std::uint8_t payload_[kMaxPayload]{};
};

}  // namespace modules
