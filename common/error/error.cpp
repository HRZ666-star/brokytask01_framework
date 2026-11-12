#include "common/error/error.h"

namespace common {

const char* to_string(Error error) noexcept {
    switch (error) {
        case Error::Ok:
            return "Ok";
        case Error::NotInitialized:
            return "NotInitialized";
        case Error::InvalidArgument:
            return "InvalidArgument";
        case Error::Timeout:
            return "Timeout";
        case Error::Busy:
            return "Busy";
        case Error::Hardware:
            return "Hardware";
        case Error::Checksum:
            return "Checksum";
        case Error::Full:
            return "Full";
        case Error::Empty:
            return "Empty";
        case Error::NotSupported:
            return "NotSupported";
        case Error::Unknown:
            return "Unknown";
    }
    return "Unknown";
}

}  // namespace common
