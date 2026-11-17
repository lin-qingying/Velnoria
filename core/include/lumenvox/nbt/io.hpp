#pragma once

#include <cstddef>
#include <string>

#include "lumenvox/core/byte_reader.hpp"
#include "lumenvox/core/byte_writer.hpp"
#include "lumenvox/core/error.hpp"
#include "lumenvox/nbt/tag.hpp"

// Wire format I/O for NBT.
//
// Every entry point is bounded: depth, total bytes, string length and
// collection size all have hard limits, because a hostile or corrupt payload
// can otherwise ask the reader to allocate gigabytes from a few bytes.

namespace lumenvox::nbt {

struct Limits {
    int max_depth = 512;
    std::size_t max_bytes = 8u * 1024u * 1024u;
    std::size_t max_string_units = 32767;
    std::size_t max_collection = 1u * 1024u * 1024u;
};

struct NamedTag {
    std::string name;
    Tag tag;
};

// Reads a full tag: type byte, name, payload.
[[nodiscard]] Result<NamedTag> read_root(ByteReader& reader, Limits limits = {});

// Reads only the payload for a known type (used by list elements and compound
// entries, which carry no name).
[[nodiscard]] Result<Tag> read_payload(ByteReader& reader, Type type, const Limits& limits);

[[nodiscard]] Status write_root(const NamedTag& named, ByteWriter& writer);
[[nodiscard]] Status write_payload(const Tag& tag, ByteWriter& writer);

}  // namespace lumenvox::nbt
