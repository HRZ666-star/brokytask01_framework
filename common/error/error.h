#pragma once

#include <cstdint>

namespace common {

/*
 * Framework-wide error codes.  Every fallible operation returns one of these
 * (directly or through Result<T>); no layer is allowed to fail silently.
 */
enum class Error : std::uint8_t {
    Ok = 0,
    NotInitialized,
    InvalidArgument,
    Timeout,
    Busy,
    Hardware,
    Checksum,
    Full,
    Empty,
    NotSupported,
    Unknown,
};

const char* to_string(Error error) noexcept;

}  // namespace common
