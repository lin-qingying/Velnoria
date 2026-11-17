#include "velnoria/core/uuid.hpp"

#include "velnoria/core/md5.hpp"

namespace velnoria {
namespace {

int hex_value(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

std::uint64_t read_u64_be(const std::uint8_t* bytes) {
    std::uint64_t value = 0;
    for (int i = 0; i < 8; ++i) {
        value = (value << 8) | bytes[i];
    }
    return value;
}

}  // namespace

Result<Uuid> Uuid::parse(std::string_view text) {
    std::uint8_t bytes[16] = {};
    int count = 0;
    int high = -1;

    for (char c : text) {
        if (c == '-') {
            continue;
        }
        const int value = hex_value(c);
        if (value < 0) {
            return Error{ErrorCode::malformed, "invalid hex digit in UUID"};
        }
        if (high < 0) {
            high = value;
        } else {
            if (count >= 16) {
                return Error{ErrorCode::malformed, "UUID longer than 32 hex digits"};
            }
            bytes[count++] = static_cast<std::uint8_t>((high << 4) | value);
            high = -1;
        }
    }

    if (high >= 0) {
        return Error{ErrorCode::malformed, "UUID has an odd number of hex digits"};
    }
    if (count != 16) {
        return Error{ErrorCode::malformed, "UUID must contain 32 hex digits"};
    }

    return Uuid(read_u64_be(bytes), read_u64_be(bytes + 8));
}

std::string Uuid::to_string() const {
    static constexpr char kDigits[] = "0123456789abcdef";
    std::string out;
    out.reserve(36);

    const auto append_byte = [&out](std::uint8_t byte) {
        out.push_back(kDigits[byte >> 4]);
        out.push_back(kDigits[byte & 0x0Fu]);
    };

    int written = 0;
    for (int half = 0; half < 2; ++half) {
        const std::uint64_t value = half == 0 ? msb_ : lsb_;
        for (int i = 7; i >= 0; --i) {
            append_byte(static_cast<std::uint8_t>((value >> (8 * i)) & 0xFFu));
            ++written;
            if (written == 4 || written == 6 || written == 8 || written == 10) {
                out.push_back('-');
            }
        }
    }
    return out;
}

Uuid Uuid::from_name_md5(std::span<const std::uint8_t> name_bytes) {
    Digest16 digest = md5(name_bytes);
    // Version 3 (name based, MD5) and the RFC 4122 variant, forced the same way
    // the original does it.
    digest[6] = static_cast<std::uint8_t>((digest[6] & 0x0Fu) | 0x30u);
    digest[8] = static_cast<std::uint8_t>((digest[8] & 0x3Fu) | 0x80u);
    return Uuid(read_u64_be(digest.data()), read_u64_be(digest.data() + 8));
}

Uuid Uuid::from_name_md5(std::string_view name) {
    return from_name_md5(std::span<const std::uint8_t>(
        reinterpret_cast<const std::uint8_t*>(name.data()), name.size()));
}

Uuid Uuid::offline_player(std::string_view name) {
    std::string key = "OfflinePlayer:";
    key.append(name);
    return from_name_md5(key);
}

}  // namespace velnoria
