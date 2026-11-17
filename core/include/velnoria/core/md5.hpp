#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

// MD5 is needed for exactly two things: offline-mode player UUIDs and the
// name-based UUIDv3 that the protocol uses for a few identifiers. It is not used
// for anything security related, and it must not be: it is here because parity
// requires the same bytes the original produces.

namespace velnoria {

using Digest16 = std::array<std::uint8_t, 16>;

[[nodiscard]] Digest16 md5(std::span<const std::uint8_t> data);
[[nodiscard]] Digest16 md5(std::string_view data);

[[nodiscard]] std::string to_hex(std::span<const std::uint8_t> bytes);

}  // namespace velnoria
