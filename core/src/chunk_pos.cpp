#include "lumenvox/world/chunk_pos.hpp"

namespace lumenvox::world {

std::uint64_t ChunkPos::packed() const noexcept {
    const auto high = static_cast<std::uint64_t>(static_cast<std::uint32_t>(x_));
    const auto low = static_cast<std::uint64_t>(static_cast<std::uint32_t>(z_));
    return (high << 32) | low;
}

ChunkPos ChunkPos::from_packed(std::uint64_t packed) noexcept {
    return ChunkPos(static_cast<std::int32_t>(packed >> 32),
                    static_cast<std::int32_t>(packed & 0xFFFFFFFFULL));
}

ChunkPos ChunkPos::from_block(std::int32_t block_x, std::int32_t block_z) noexcept {
    // Arithmetic shift == floor division by 16 for two's complement ints.
    return ChunkPos(block_x >> kBlockBitsPerAxis, block_z >> kBlockBitsPerAxis);
}

std::size_t ChunkPos::hash() const noexcept {
    std::uint64_t value = packed();
    value ^= value >> 33;
    value *= 0xff51afd7ed558ccdULL;
    value ^= value >> 33;
    return static_cast<std::size_t>(value);
}

}  // namespace lumenvox::world
