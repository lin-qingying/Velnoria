#include "lumenvox/core/error.hpp"

namespace lumenvox {

std::string_view to_string(ErrorCode code) noexcept {
    switch (code) {
        case ErrorCode::truncated:
            return "truncated";
        case ErrorCode::varint_too_long:
            return "varint_too_long";
        case ErrorCode::varint_overflow:
            return "varint_overflow";
        case ErrorCode::frame_too_large:
            return "frame_too_large";
        case ErrorCode::compression_unsupported:
            return "compression_unsupported";
        case ErrorCode::compression_failed:
            return "compression_failed";
        case ErrorCode::value_out_of_range:
            return "value_out_of_range";
        case ErrorCode::malformed:
            return "malformed";
        case ErrorCode::unsupported:
            return "unsupported";
    }
    return "unknown";
}

}  // namespace lumenvox
