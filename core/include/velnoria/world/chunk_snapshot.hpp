#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "velnoria/core/error.hpp"
#include "velnoria/world/chunk_pos.hpp"

// The only structure the renderer is allowed to read. Immutable after publish,
// handed across threads by shared_ptr, never mutated in place.
//
// Layout matches the wire/storage layout so decoding does not copy:
//   block_states : one u16 global state id per block, section-major
//   sky_light    : nibble packed, 2048 bytes per section (low nibble = even index)
//   block_light  : same shape as sky_light

namespace velnoria::world {

inline constexpr std::size_t kSectionVolume =
    static_cast<std::size_t>(kChunkSize) * kChunkSize * kSectionHeight;
inline constexpr std::size_t kLightBytesPerSection = kSectionVolume / 2;

class ChunkSnapshot {
public:
    [[nodiscard]] ChunkPos pos() const noexcept { return pos_; }
    [[nodiscard]] int section_count() const noexcept { return section_count_; }
    [[nodiscard]] std::int32_t min_section_y() const noexcept { return min_section_y_; }

    [[nodiscard]] const std::vector<std::uint16_t>& block_states() const noexcept {
        return block_states_;
    }
    [[nodiscard]] const std::vector<std::uint8_t>& sky_light() const noexcept { return sky_light_; }
    [[nodiscard]] const std::vector<std::uint8_t>& block_light() const noexcept {
        return block_light_;
    }

    // Monotonic counters: generation changes on any block change, tick is the
    // simulation tick that produced it. Renderer discards stale snapshots
    // instead of blocking.
    [[nodiscard]] std::uint64_t generation() const noexcept { return generation_; }
    [[nodiscard]] std::uint64_t tick() const noexcept { return tick_; }

    struct Build {
        ChunkPos pos;
        int section_count = 0;
        std::int32_t min_section_y = 0;
        std::vector<std::uint16_t> block_states;
        std::vector<std::uint8_t> sky_light;
        std::vector<std::uint8_t> block_light;
        std::uint64_t generation = 0;
        std::uint64_t tick = 0;
    };

    // Validates array sizes against section_count before publishing. A size
    // mismatch is a decoder bug, surfaced here rather than as out of range
    // reads during meshing.
    [[nodiscard]] static Result<std::shared_ptr<const ChunkSnapshot>> create(Build build);

private:
    ChunkSnapshot() = default;

    ChunkPos pos_{};
    int section_count_ = 0;
    std::int32_t min_section_y_ = 0;
    std::vector<std::uint16_t> block_states_;
    std::vector<std::uint8_t> sky_light_;
    std::vector<std::uint8_t> block_light_;
    std::uint64_t generation_ = 0;
    std::uint64_t tick_ = 0;
};

using ChunkSnapshotPtr = std::shared_ptr<const ChunkSnapshot>;

}  // namespace velnoria::world
