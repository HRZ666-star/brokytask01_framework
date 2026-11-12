#pragma once

#include "common/error/error.h"

#include <type_traits>
#include <utility>

namespace common {

/*
 * A small, exception-free result type.
 *
 * T is required to be default constructible and assignable.  That is enough
 * for the value types used by this framework (sizes, timestamps, small PODs);
 * keeping the implementation this small avoids pulling <optional> and its
 * exception machinery into the firmware.
 */
template <typename T>
class [[nodiscard]] Result {
    static_assert(!std::is_void<T>::value, "use Result<void> for void results");
    static_assert(std::is_default_constructible<T>::value,
                  "Result<T> requires a default-constructible T");
    static_assert(std::is_copy_assignable<T>::value,
                  "Result<T> requires an assignable T");

public:
    static Result success(const T& value) {
        Result result;
        result.value_ = value;
        result.error_ = Error::Ok;
        result.ok_ = true;
        return result;
    }

    static Result success(T&& value) {
        Result result;
        result.value_ = std::move(value);
        result.error_ = Error::Ok;
        result.ok_ = true;
        return result;
    }

    static Result failure(Error error) {
        Result result;
        result.error_ = error;
        result.ok_ = false;
        return result;
    }

    [[nodiscard]] bool has_value() const noexcept { return ok_; }
    [[nodiscard]] explicit operator bool() const noexcept { return ok_; }

    [[nodiscard]] T& value() noexcept { return value_; }
    [[nodiscard]] const T& value() const noexcept { return value_; }

    [[nodiscard]] Error error() const noexcept { return error_; }

private:
    T value_{};
    Error error_{Error::Unknown};
    bool ok_{false};
};

template <>
class [[nodiscard]] Result<void> {
public:
    static Result success() {
        Result result;
        result.error_ = Error::Ok;
        result.ok_ = true;
        return result;
    }

    static Result failure(Error error) {
        Result result;
        result.error_ = error;
        result.ok_ = false;
        return result;
    }

    [[nodiscard]] bool has_value() const noexcept { return ok_; }
    [[nodiscard]] explicit operator bool() const noexcept { return ok_; }
    [[nodiscard]] Error error() const noexcept { return error_; }

private:
    Error error_{Error::Unknown};
    bool ok_{false};
};

}  // namespace common
