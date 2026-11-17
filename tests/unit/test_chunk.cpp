#include <cstdint>
#include <vector>

#include "velnoria/world/chunk_pos.hpp"
#include "velnoria/world/chunk_snapshot.hpp"
#include "test_harness.hpp"

using velnoria::world::ChunkPos;
using velnoria::world::ChunkSnapshot;
using velnoria::world::kLightBytesPerSection;
using velnoria::world::kSectionVolume;

namespace {

ChunkSnapshot::Build make_build(int sections) {
    ChunkSnapshot::Build build;
    build.pos = ChunkPos(3, -7);
    build.section_count = sections;
    build.min_section_y = -4;
    build.block_states.assign(static_cast<std::size_t>(sections) * kSectionVolume, 0u);
    build.sky_light.assign(static_cast<std::size_t>(sections) * kLightBytesPerSection, 0xFFu);
    build.block_light.assign(static_cast<std::size_t>(sections) * kLightBytesPerSection, 0x00u);
    build.generation = 42;
    build.tick = 1000;
    return build;
}

}  // namespace

LV_TEST(chunk_pos_pack_roundtrip_including_negative) {
    const ChunkPos cases[] = {ChunkPos(0, 0),      ChunkPos(1, 2),        ChunkPos(-1, -2),
                              ChunkPos(2147483647, -2147483647 - 1)};

    for (const auto& pos : cases) {
        const auto restored = ChunkPos::from_packed(pos.packed());
        LV_REQUIRE(restored == pos);
    }
}

LV_TEST(chunk_pos_from_block_uses_floor_division) {
    LV_REQUIRE(ChunkPos::from_block(0, 0) == ChunkPos(0, 0));
    LV_REQUIRE(ChunkPos::from_block(15, 15) == ChunkPos(0, 0));
    LV_REQUIRE(ChunkPos::from_block(16, 32) == ChunkPos(1, 2));
    // -1 belongs to chunk -1, not chunk 0: truncation toward zero would be wrong.
    LV_REQUIRE(ChunkPos::from_block(-1, -17) == ChunkPos(-1, -2));
}

LV_TEST(chunk_pos_hash_distinguishes_axes) {
    LV_REQUIRE(ChunkPos(1, 0).hash() != ChunkPos(0, 1).hash());
    LV_REQUIRE(ChunkPos(5, 5).hash() == ChunkPos(5, 5).hash());
}

LV_TEST(chunk_snapshot_accepts_consistent_sizes) {
    auto snapshot = ChunkSnapshot::create(make_build(24));
    LV_REQUIRE(snapshot.ok());
    const auto& value = snapshot.value();
    LV_REQUIRE(value->section_count() == 24);
    LV_REQUIRE(value->pos() == ChunkPos(3, -7));
    LV_CHECK_EQ(value->generation(), 42ULL);
    LV_CHECK_EQ(value->tick(), 1000ULL);
    LV_CHECK_EQ(value->block_states().size(), 24u * kSectionVolume);
}

LV_TEST(chunk_snapshot_rejects_mismatched_light_arrays) {
    auto build = make_build(4);
    build.sky_light.pop_back();
    LV_CHECK_ERR(ChunkSnapshot::create(std::move(build)), velnoria::ErrorCode::value_out_of_range);
}

LV_TEST(chunk_snapshot_rejects_zero_sections) {
    auto build = make_build(1);
    build.section_count = 0;
    LV_CHECK_ERR(ChunkSnapshot::create(std::move(build)), velnoria::ErrorCode::malformed);
}
