#include "velnoria/protocol/types.hpp"

#include <cstring>
#include <vector>

#include "velnoria/core/utf8.hpp"
#include "velnoria/core/varint.hpp"

namespace velnoria::protocol {

Result<std::string> read_string(ByteReader& reader, std::size_t max_units) {
    auto length = read_varint(reader);
    if (!length) {
        return length.error();
    }
    const auto byte_length = static_cast<std::size_t>(length.value());

    // Reject before reading: the prefix is attacker controlled.
    if (byte_length > max_units * 4u) {
        return Error{ErrorCode::value_out_of_range, "string byte length exceeds limit"};
    }

    auto bytes = reader.read_bytes(byte_length);
    if (!bytes) {
        return bytes.error();
    }

    const std::string_view view(reinterpret_cast<const char*>(bytes.value().data()),
                               bytes.value().size());
    std::size_t units = 0;
    if (!utf8::validate_and_count(view, units)) {
        return Error{ErrorCode::malformed, "string is not valid UTF-8"};
    }
    if (units > max_units) {
        return Error{ErrorCode::value_out_of_range, "string exceeds code unit limit"};
    }

    return std::string(view);
}

Status write_string(std::string_view value, ByteWriter& writer, std::size_t max_units) {
    std::size_t units = 0;
    if (!utf8::validate_and_count(value, units)) {
        return Status::fail(ErrorCode::malformed, "refusing to write invalid UTF-8");
    }
    if (units > max_units) {
        return Status::fail(ErrorCode::value_out_of_range, "string exceeds code unit limit");
    }
    write_varint(static_cast<std::uint32_t>(value.size()), writer);
    writer.write_bytes(std::span<const std::uint8_t>(
        reinterpret_cast<const std::uint8_t*>(value.data()), value.size()));
    return Status::success();
}

Result<BlockPos> read_position(ByteReader& reader) {
    auto raw = read_i64(reader);
    if (!raw) {
        return raw.error();
    }
    const auto packed = static_cast<std::uint64_t>(raw.value());

    // Sign extend the packed fields: 26 bits for x, 26 for z, 12 for y.
    constexpr std::uint64_t kXMask = (1ULL << 26) - 1;
    constexpr std::uint64_t kZMask = (1ULL << 26) - 1;
    constexpr std::uint64_t kYMask = (1ULL << 12) - 1;

    const auto sign_extend = [](std::uint64_t value, int bits) -> std::int32_t {
        const std::uint64_t sign_bit = 1ULL << (bits - 1);
        const std::uint64_t mask = (1ULL << bits) - 1;
        value &= mask;
        return static_cast<std::int32_t>((value ^ sign_bit) - sign_bit);
    };

    BlockPos pos;
    pos.x = sign_extend((packed >> 38) & kXMask, 26);
    pos.z = sign_extend((packed >> 12) & kZMask, 26);
    pos.y = sign_extend(packed & kYMask, 12);
    return pos;
}

Status write_position(BlockPos pos, ByteWriter& writer) {
    // Truncate, do not saturate: the original stores the low 26 / 12 bits.
    const auto x = static_cast<std::uint64_t>(pos.x) & ((1ULL << 26) - 1);
    const auto z = static_cast<std::uint64_t>(pos.z) & ((1ULL << 26) - 1);
    const auto y = static_cast<std::uint64_t>(pos.y) & ((1ULL << 12) - 1);
    const std::uint64_t packed = (x << 38) | (z << 12) | y;
    return write_i64(static_cast<std::int64_t>(packed), writer);
}

Result<std::uint16_t> read_u16(ByteReader& reader) {
    auto bytes = reader.read_bytes(2);
    if (!bytes) {
        return bytes.error();
    }
    const auto* data = bytes.value().data();
    return static_cast<std::uint16_t>((data[0] << 8) | data[1]);
}

Status write_u16(std::uint16_t value, ByteWriter& writer) {
    writer.write_u8(static_cast<std::uint8_t>(value >> 8));
    writer.write_u8(static_cast<std::uint8_t>(value & 0xFFu));
    return Status::success();
}

Result<std::int64_t> read_i64(ByteReader& reader) {
    auto bytes = reader.read_bytes(8);
    if (!bytes) {
        return bytes.error();
    }
    std::uint64_t value = 0;
    for (int i = 0; i < 8; ++i) {
        value = (value << 8) | bytes.value()[i];
    }
    return static_cast<std::int64_t>(value);
}

Status write_i64(std::int64_t value, ByteWriter& writer) {
    const auto unsigned_value = static_cast<std::uint64_t>(value);
    for (int shift = 56; shift >= 0; shift -= 8) {
        writer.write_u8(static_cast<std::uint8_t>((unsigned_value >> shift) & 0xFFu));
    }
    return Status::success();
}

Result<Uuid> read_uuid(ByteReader& reader) {
    auto msb = read_i64(reader);
    if (!msb) {
        return msb.error();
    }
    auto lsb = read_i64(reader);
    if (!lsb) {
        return lsb.error();
    }
    return Uuid(static_cast<std::uint64_t>(msb.value()), static_cast<std::uint64_t>(lsb.value()));
}

Status write_uuid(const Uuid& uuid, ByteWriter& writer) {
    auto status = write_i64(static_cast<std::int64_t>(uuid.msb()), writer);
    if (!status) {
        return status;
    }
    return write_i64(static_cast<std::int64_t>(uuid.lsb()), writer);
}

}  // namespace velnoria::protocol
