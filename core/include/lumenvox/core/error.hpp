#pragma once

#include <string>
#include <string_view>
#include <utility>
#include <variant>

namespace lumenvox {

// Error taxonomy is intentionally coarse: callers switch on the code, the
// detail string carries context for logs and differential reports.
enum class ErrorCode {
    truncated,
    varint_too_long,
    varint_overflow,
    frame_too_large,
    compression_unsupported,
    compression_failed,
    value_out_of_range,
    malformed,
    unsupported,
};

struct Error {
    ErrorCode code;
    std::string detail;
};

[[nodiscard]] std::string_view to_string(ErrorCode code) noexcept;

class Status {
public:
    Status() = default;

    static Status success() { return Status{}; }
    static Status fail(ErrorCode code, std::string detail) {
        Status s;
        s.error_ = Error{code, std::move(detail)};
        s.ok_ = false;
        return s;
    }

    [[nodiscard]] bool ok() const noexcept { return ok_; }
    explicit operator bool() const noexcept { return ok_; }
    [[nodiscard]] const Error& error() const noexcept { return error_; }

private:
    bool ok_ = true;
    Error error_{ErrorCode::malformed, {}};
};

template <typename T>
class Result {
public:
    Result(T value) : storage_(std::move(value)) {}
    Result(Error err) : storage_(std::move(err)) {}

    [[nodiscard]] bool ok() const noexcept { return storage_.index() == 0; }
    explicit operator bool() const noexcept { return ok(); }

    [[nodiscard]] const T& value() const& { return std::get<0>(storage_); }
    [[nodiscard]] T& value() & { return std::get<0>(storage_); }
    [[nodiscard]] T take() && { return std::move(std::get<0>(storage_)); }
    [[nodiscard]] const Error& error() const& { return std::get<1>(storage_); }

private:
    std::variant<T, Error> storage_;
};

}  // namespace lumenvox
