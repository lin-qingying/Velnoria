#include "velnoria/core/byte_reader.hpp"

namespace velnoria {

Result<std::uint8_t> ByteReader::read_u8() {
    if (remaining() < 1) {
        return Error{ErrorCode::truncated, "read_u8 past end of buffer"};
    }
    return data_[pos_++];
}

Result<std::span<const std::uint8_t>> ByteReader::read_bytes(std::size_t count) {
    if (remaining() < count) {
        return Error{ErrorCode::truncated, "read_bytes past end of buffer"};
    }
    const auto slice = data_.subspan(pos_, count);
    pos_ += count;
    return slice;
}

Status ByteReader::skip(std::size_t count) {
    if (remaining() < count) {
        return Status::fail(ErrorCode::truncated, "skip past end of buffer");
    }
    pos_ += count;
    return Status::success();
}

}  // namespace velnoria
