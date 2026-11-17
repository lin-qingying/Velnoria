#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "velnoria/core/error.hpp"

// Block state registry: the global state id <-> (block, properties) table that
// chunk decoding depends on. Loaded from the TSV written by prepare, never
// hand written, and never edited at runtime.

namespace velnoria::world {

struct BlockStateEntry {
    std::uint32_t id = 0;
    std::string block;
    std::string properties;  // "k=v,k=v" in source order, may be empty
};

class BlockStateRegistry {
public:
    // Content of block_states.tsv. Ids must be dense starting at zero: a hole
    // means the export was truncated, which must fail loudly rather than shift
    // every later state.
    [[nodiscard]] Status load_tsv(std::string_view content);

    [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }
    [[nodiscard]] const BlockStateEntry* find(std::uint32_t id) const;
    [[nodiscard]] const std::vector<BlockStateEntry>& entries() const noexcept { return entries_; }

private:
    std::vector<BlockStateEntry> entries_;
};

}  // namespace velnoria::world
