#include "velnoria/world/chunk_snapshot.hpp"

#include <utility>

namespace velnoria::world {

Result<ChunkSnapshotPtr> ChunkSnapshot::create(Build build) {
    if (build.section_count <= 0) {
        return Error{ErrorCode::malformed, "chunk snapshot requires at least one section"};
    }

    const auto sections = static_cast<std::size_t>(build.section_count);
    if (build.block_states.size() != sections * kSectionVolume) {
        return Error{ErrorCode::value_out_of_range,
                     "block state array size does not match section count"};
    }
    if (build.sky_light.size() != sections * kLightBytesPerSection) {
        return Error{ErrorCode::value_out_of_range,
                     "sky light array size does not match section count"};
    }
    if (build.block_light.size() != sections * kLightBytesPerSection) {
        return Error{ErrorCode::value_out_of_range,
                     "block light array size does not match section count"};
    }

    auto snapshot = ChunkSnapshotPtr(new ChunkSnapshot());
    // const_cast is confined to construction; after this point the object is
    // only reachable as shared_ptr<const T>.
    auto* mutable_view = const_cast<ChunkSnapshot*>(snapshot.get());
    mutable_view->pos_ = build.pos;
    mutable_view->section_count_ = build.section_count;
    mutable_view->min_section_y_ = build.min_section_y;
    mutable_view->block_states_ = std::move(build.block_states);
    mutable_view->sky_light_ = std::move(build.sky_light);
    mutable_view->block_light_ = std::move(build.block_light);
    mutable_view->generation_ = build.generation;
    mutable_view->tick_ = build.tick;

    return snapshot;
}

}  // namespace velnoria::world
