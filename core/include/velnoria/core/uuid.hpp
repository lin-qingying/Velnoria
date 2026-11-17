#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>

#include "velnoria/core/error.hpp"

namespace velnoria {

// 128 bit identifier with the byte order the protocol uses: most significant
// half first, each half big endian on the wire.
class Uuid {
public:
    Uuid() = default;
    Uuid(std::uint64_t msb, std::uint64_t lsb) : msb_(msb), lsb_(lsb) {}

    [[nodiscard]] std::uint64_t msb() const noexcept { return msb_; }
    [[nodiscard]] std::uint64_t lsb() const noexcept { return lsb_; }

    // Accepts 8-4-4-4-12 and the hyphen free 32 character form.
    [[nodiscard]] static Result<Uuid> parse(std::string_view text);

    [[nodiscard]] std::string to_string() const;

    // Field layout: version in bits 12..15 of the most significant half, variant
    // in bits 62..63 of the least significant half.
    [[nodiscard]] int version() const noexcept {
        return static_cast<int>((msb_ >> 12) & 0x0FULL);
    }

    // Name based UUID as produced by the original for offline players: MD5 over
    // the raw name bytes, then the version 3 / RFC 4122 variant bits forced in.
    [[nodiscard]] static Uuid from_name_md5(std::span<const std::uint8_t> name_bytes);
    [[nodiscard]] static Uuid from_name_md5(std::string_view name);

    // Offline mode identity: nameMD5("OfflinePlayer:" + name).
    [[nodiscard]] static Uuid offline_player(std::string_view name);

    [[nodiscard]] bool operator==(const Uuid& other) const noexcept {
        return msb_ == other.msb_ && lsb_ == other.lsb_;
    }
    [[nodiscard]] bool operator!=(const Uuid& other) const noexcept { return !(*this == other); }

private:
    std::uint64_t msb_ = 0;
    std::uint64_t lsb_ = 0;
};

}  // namespace velnoria
