#include "modules/protocol/frame_parser.h"

#include <cstring>

namespace modules {

std::uint8_t FrameParser::crc8_update(std::uint8_t crc,
                                      std::uint8_t byte) noexcept {
    crc ^= byte;
    for (std::uint8_t bit = 0U; bit < 8U; ++bit) {
        if ((crc & 0x80U) != 0U) {
            crc = static_cast<std::uint8_t>((crc << 1U) ^ 0x07U);
        } else {
            crc = static_cast<std::uint8_t>(crc << 1U);
        }
    }
    return crc;
}

void FrameParser::reset() noexcept {
    state_ = State::WaitA;
    length_ = 0U;
    index_ = 0U;
    command_ = 0U;
    crc_ = 0U;
}

bool FrameParser::feed(std::uint8_t byte, Frame& out) noexcept {
    switch (state_) {
        case State::WaitA:
            if (byte == 0xAAU) {
                state_ = State::Wait55;
            }
            return false;

        case State::Wait55:
            if (byte == 0x55U) {
                state_ = State::Length;
            } else if (byte != 0xAAU) {
                state_ = State::WaitA;
            }
            return false;

        case State::Length:
            if (byte > kMaxPayload) {
                reset();
                return false;
            }
            length_ = byte;
            index_ = 0U;
            crc_ = crc8_update(0U, byte);
            state_ = (byte == 0U) ? State::Crc : State::Command;
            return false;

        case State::Command:
            command_ = byte;
            crc_ = crc8_update(crc_, byte);
            state_ = (length_ == 0U) ? State::Crc : State::Payload;
            return false;

        case State::Payload:
            payload_[index_++] = byte;
            crc_ = crc8_update(crc_, byte);
            if (index_ >= length_) {
                state_ = State::Crc;
            }
            return false;

        case State::Crc:
            if (byte == crc_) {
                out.command = command_;
                out.length = length_;
                std::memcpy(out.payload, payload_, length_);
                reset();
                return true;
            }
            reset();
            return false;
    }

    reset();
    return false;
}

}  // namespace modules
