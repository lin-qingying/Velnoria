#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "velnoria/core/error.hpp"
#include "velnoria/protocol/state.hpp"

// Packet ids are never literals in source. They arrive as a generated manifest
// produced by the prepare tool from the user's own jar, so a protocol bump is a
// data change rather than a code change.
//
// The manifest format is versioned; see docs/design/vertical-slice-01.md section 4.

namespace velnoria::protocol {

struct PacketEntry {
    State state;
    Bound bound;
    std::string name;
    std::int32_t id;
    // Byte width hint used by the decoder to reject obviously wrong frames
    // before touching fields. Zero means "unbounded / variable".
    std::uint32_t fixed_width = 0;
};

class PacketTable {
public:
    // Returns an error if the entry duplicates an existing (state, bound, name)
    // key or if the id is negative.
    Status add(PacketEntry entry);

    [[nodiscard]] Result<std::int32_t> id_of(State state, Bound bound, std::string_view name) const;
    [[nodiscard]] Result<std::string_view> name_of(State state, Bound bound, std::int32_t id) const;
    [[nodiscard]] std::size_t size() const noexcept { return id_by_name_.size(); }

    // Order insensitive digest used to detect manifest drift at startup.
    [[nodiscard]] std::uint64_t content_hash() const;

    // Content of the packets.tsv written by prepare. Existing entries are kept,
    // so a table can accumulate several manifests; duplicates are rejected by
    // add() and surface as an error here.
    [[nodiscard]] Status load_tsv(std::string_view content);

private:
    struct Key {
        State state;
        Bound bound;
        std::string name;

        bool operator==(const Key& other) const noexcept {
            return state == other.state && bound == other.bound && name == other.name;
        }
    };

    struct KeyHash {
        std::size_t operator()(const Key& key) const noexcept;
    };

    struct IdKey {
        State state;
        Bound bound;
        std::int32_t id;

        bool operator==(const IdKey& other) const noexcept {
            return state == other.state && bound == other.bound && id == other.id;
        }
    };

    struct IdKeyHash {
        std::size_t operator()(const IdKey& key) const noexcept;
    };

    std::vector<PacketEntry> entries_;
    std::unordered_map<Key, std::int32_t, KeyHash> id_by_name_;
    std::unordered_map<IdKey, std::string, IdKeyHash> name_by_id_;
};

}  // namespace velnoria::protocol
