#include "lumenvox/core/varint.hpp"

namespace lumenvox {
namespace {

template <typename UInt>
Result<UInt> read_var_impl(ByteReader& reader, int max_bytes) {
    UInt result = 0;
    int shift = 0;

    for (int index = 0; index < max_bytes; ++index) {
        auto byte = reader.read_u8();
        if (!byte) {
            return byte.error();
        }
        const auto payload = static_cast<UInt>(byte.value() & 0x7Fu);

        // On the last byte a given width can carry, distinguish the two failure
        // modes: a continuation bit means the encoding is longer than allowed,
        // extra payload bits mean it does not fit the target width.
        constexpr int bits = static_cast<int>(sizeof(UInt) * 8);
        if (shift + 7 > bits) {
            if ((byte.value() & 0x80u) != 0) {
                return Error{ErrorCode::varint_too_long,
                             "varint continued past maximum byte count"};
            }
            if ((payload >> (bits - shift)) != 0) {
                return Error{ErrorCode::varint_overflow,
                             "varint payload exceeds target width"};
            }
        }

        result |= static_cast<UInt>(payload << shift);
        if ((byte.value() & 0x80u) == 0) {
            return result;
        }
        shift += 7;
    }

    return Error{ErrorCode::varint_too_long, "varint continued past maximum byte count"};
}

template <typename UInt>
void write_var_impl(UInt value, ByteWriter& writer) {
    while (true) {
        if ((value & ~static_cast<UInt>(0x7Fu)) == 0) {
            writer.write_u8(static_cast<std::uint8_t>(value));
            return;
        }
        writer.write_u8(static_cast<std::uint8_t>((value & 0x7Fu) | 0x80u));
        value >>= 7;
    }
}

}  // namespace

Result<std::uint32_t> read_varint(ByteReader& reader) {
    return read_var_impl<std::uint32_t>(reader, kMaxVarIntBytes);
}

Result<std::uint64_t> read_varlong(ByteReader& reader) {
    return read_var_impl<std::uint64_t>(reader, kMaxVarLongBytes);
}

void write_varint(std::uint32_t value, ByteWriter& writer) {
    write_var_impl<std::uint32_t>(value, writer);
}

void write_varlong(std::uint64_t value, ByteWriter& writer) {
    write_var_impl<std::uint64_t>(value, writer);
}

std::size_t varint_size(std::uint32_t value) noexcept {
    std::size_t size = 1;
    while ((value & ~0x7Fu) != 0) {
        value >>= 7;
        ++size;
    }
    return size;
}

}  // namespace lumenvox
