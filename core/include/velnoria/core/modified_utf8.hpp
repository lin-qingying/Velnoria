#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "velnoria/core/error.hpp"

// Java's "modified UTF-8", which is what NBT strings and chat components use on
// the wire. It differs from standard UTF-8 in two ways that matter for byte
// level parity:
//   - U+0000 is encoded as C0 80, never as 00
//   - characters above U+FFFF are written as two three byte surrogates, so no
//     four byte form ever appears
// Accepting standard UTF-8 here and re-encoding it would silently corrupt any
// string containing NUL or an astral character, so both directions are strict.

namespace velnoria::text {

inline constexpr std::size_t kMaxModifiedUtf8Units = 32767;

Result<std::string> decode_modified_utf8(std::span<const std::uint8_t> bytes,
                                         std::size_t max_units = kMaxModifiedUtf8Units);

Status encode_modified_utf8(std::string_view utf8, std::vector<std::uint8_t>& out,
                            std::size_t max_units = kMaxModifiedUtf8Units);

// Number of UTF-16 code units the string needs, which is the length prefix value
// NBT stores. Returns 0 for invalid UTF-8.
[[nodiscard]] std::size_t modified_utf8_unit_count(std::string_view utf8);

}  // namespace velnoria::text
