#pragma once

#include <cstdint>

#include "velnoria/core/byte_reader.hpp"
#include "velnoria/core/byte_writer.hpp"
#include "velnoria/core/error.hpp"

// Variable length integers, the base scalar of every Minecraft protocol frame.
// The decoder is strict on purpose: overlong encodings and truncated tails are
// rejected rather than normalised, because loose decoding hides upstream bugs
// that only surface as desync against a real server.

namespace velnoria {

inline constexpr int kMaxVarIntBytes = 5;
inline constexpr int kMaxVarLongBytes = 10;

Result<std::uint32_t> read_varint(ByteReader& reader);
Result<std::uint64_t> read_varlong(ByteReader& reader);

void write_varint(std::uint32_t value, ByteWriter& writer);
void write_varlong(std::uint64_t value, ByteWriter& writer);

// Convenience helpers used by tests and small tooling.
[[nodiscard]] std::size_t varint_size(std::uint32_t value) noexcept;

}  // namespace velnoria
