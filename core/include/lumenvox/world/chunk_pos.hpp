#pragma once

#include <cstdint>
#include <functional>

#include "lumenvox/core/error.hpp"

namespace lumenvox::world {

inline constexpr int kChunkSize = 16;
inline constexpr int kSectionHeight = 16;
inline constexpr int kBlockBitsPerAxis = 4;
inline constexpr int kBlockIndexMask = 0xF;

// Chunk column coordinates. Packed form is a single u64 (x high, z low) as used
// on the wire and in region files.
class ChunkPos {
public:
    ChunkPos() = default;
    ChunkPos(std::int32_t x, std::int32_t z) : x_(x), z_(z) {}

    [[nodiscard]] std::int32_t x() const noexcept { return x_; }
    [[nodiscard]] std::int32_t z() const noexcept { return z_; }

    [[nodiscard]] std::uint64_t packed() const noexcept;
    [[nodiscard]] static ChunkPos from_packed(std::uint64_t packed) noexcept;

    // Block coordinates -> containing chunk. Uses arithmetic shift semantics,
    // matching vanilla floor division for negative coordinates.
    [[nodiscard]] static ChunkPos from_block(std::int32_t block_x, std::int32_t block_z) noexcept;

    [[nodiscard]] std::size_t hash() const noexcept;
    [[nodiscard]] bool operator==(const ChunkPos& other) const noexcept {
        return x_ == other.x_ && z_ == other.z_;
    }
    [[nodiscard]] bool operator!=(const ChunkPos& other) const noexcept {
        return !(*this == other);
    }

private:
    std::int32_t x_ = 0;
    std::int32_t z_ = 0;
};

}  // namespace lumenvox::world

namespace std {
template <>
struct hash<lumenvox::world::ChunkPos> {
    std::size_t operator()(const lumenvox::world::ChunkPos& pos) const noexcept { return pos.hash(); }
};
}  // namespace std
