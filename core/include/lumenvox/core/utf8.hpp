#pragma once

#include <cstddef>
#include <string_view>

// Standard UTF-8, as used by protocol string fields. Deliberately separate from
// text::decode_modified_utf8: mixing the two is the single most likely source of
// silent byte differences, so each has its own entry point and its own tests.

namespace lumenvox::utf8 {

// Validates the sequence and reports how many UTF-16 code units it needs, which
// is the quantity protocol string limits are expressed in. Returns false for any
// malformed input, including overlong forms and lone continuation bytes.
[[nodiscard]] bool validate_and_count(std::string_view text, std::size_t& units);

}  // namespace lumenvox::utf8
