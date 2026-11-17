#include "velnoria/protocol/packet_table.hpp"

#include <functional>
#include <utility>

namespace velnoria::protocol {
namespace {

std::uint64_t mix(std::uint64_t hash, std::uint64_t value) {
    hash ^= value + 0x9e3779b97f4a7c15ULL + (hash << 6) + (hash >> 2);
    return hash;
}

}  // namespace

std::size_t PacketTable::KeyHash::operator()(const Key& key) const noexcept {
    std::size_t hash = static_cast<std::size_t>(key.state) * 131u;
    hash = hash * 131u + static_cast<std::size_t>(key.bound);
    hash ^= std::hash<std::string>{}(key.name) + 0x9e3779b9u + (hash << 6) + (hash >> 2);
    return hash;
}

std::size_t PacketTable::IdKeyHash::operator()(const IdKey& key) const noexcept {
    std::size_t hash = static_cast<std::size_t>(key.state) * 131u;
    hash = hash * 131u + static_cast<std::size_t>(key.bound);
    hash = hash * 131u + static_cast<std::size_t>(key.id);
    return hash;
}

Status PacketTable::add(PacketEntry entry) {
    if (entry.id < 0) {
        return Status::fail(ErrorCode::value_out_of_range, "negative packet id for " + entry.name);
    }

    Key key{entry.state, entry.bound, entry.name};
    if (id_by_name_.find(key) != id_by_name_.end()) {
        return Status::fail(ErrorCode::malformed, "duplicate packet name " + entry.name);
    }

    IdKey id_key{entry.state, entry.bound, entry.id};
    if (name_by_id_.find(id_key) != name_by_id_.end()) {
        return Status::fail(ErrorCode::malformed,
                            "duplicate packet id " + std::to_string(entry.id) + " for " + entry.name);
    }

    id_by_name_.emplace(key, entry.id);
    name_by_id_.emplace(id_key, entry.name);
    entries_.push_back(std::move(entry));
    return Status::success();
}

Result<std::int32_t> PacketTable::id_of(State state, Bound bound, std::string_view name) const {
    auto it = id_by_name_.find(Key{state, bound, std::string(name)});
    if (it == id_by_name_.end()) {
        return Error{ErrorCode::unsupported, "unknown packet " + std::string(name)};
    }
    return it->second;
}

Result<std::string_view> PacketTable::name_of(State state, Bound bound, std::int32_t id) const {
    auto it = name_by_id_.find(IdKey{state, bound, id});
    if (it == name_by_id_.end()) {
        return Error{ErrorCode::unsupported,
                     "unknown packet id " + std::to_string(id) + " in state " +
                         std::to_string(static_cast<int>(state))};
    }
    return std::string_view(it->second);
}

std::uint64_t PacketTable::content_hash() const {
    // Sum of per-entry hashes so insertion order does not matter.
    std::uint64_t total = 0;
    for (const auto& entry : entries_) {
        std::uint64_t hash = 0xcbf29ce484222325ULL;
        hash = mix(hash, static_cast<std::uint64_t>(entry.state));
        hash = mix(hash, static_cast<std::uint64_t>(entry.bound));
        hash = mix(hash, static_cast<std::uint64_t>(entry.id));
        for (char c : entry.name) {
            hash = mix(hash, static_cast<std::uint64_t>(static_cast<unsigned char>(c)));
        }
        total += hash;
    }
    return total;
}

}  // namespace velnoria::protocol
