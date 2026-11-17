#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "velnoria/core/byte_reader.hpp"
#include "velnoria/core/byte_writer.hpp"
#include "velnoria/core/error.hpp"
#include "velnoria/core/uuid.hpp"

// Protocol level scalar types. Each one has a decoding contract fixed by the
// specification in docs/protocol/types/scalars.md; none of them trust their
// input to have already been validated.

namespace velnoria::protocol {

inline constexpr std::size_t kMaxStringUnits = 32767;

// VarInt byte length prefix, standard UTF-8 payload, limit counted in UTF-16
// code units. The byte length is additionally capped at 4 * limit before the
// buffer is read, so an oversized prefix cannot be used to force an allocation.
[[nodiscard]] Result<std::string> read_string(ByteReader& reader,
                                              std::size_t max_units = kMaxStringUnits);
[[nodiscard]] Status write_string(std::string_view value, ByteWriter& writer,
                                  std::size_t max_units = kMaxStringUnits);

struct BlockPos {
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t z = 0;

    bool operator==(const BlockPos&) const = default;
};

[[nodiscard]] Result<BlockPos> read_position(ByteReader& reader);
[[nodiscard]] Status write_position(BlockPos pos, ByteWriter& writer);

[[nodiscard]] Result<std::uint16_t> read_u16(ByteReader& reader);
[[nodiscard]] Status write_u16(std::uint16_t value, ByteWriter& writer);

[[nodiscard]] Result<std::int64_t> read_i64(ByteReader& reader);
[[nodiscard]] Status write_i64(std::int64_t value, ByteWriter& writer);

[[nodiscard]] Result<Uuid> read_uuid(ByteReader& reader);
[[nodiscard]] Status write_uuid(const Uuid& uuid, ByteWriter& writer);

}  // namespace velnoria::protocol
