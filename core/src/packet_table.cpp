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

namespace {

std::vector<std::string_view> split_tabs(std::string_view line) {
    std::vector<std::string_view> fields;
    std::size_t start = 0;
    while (true) {
        const std::size_t tab = line.find('\t', start);
        if (tab == std::string_view::npos) {
            fields.push_back(line.substr(start));
            return fields;
        }
        fields.push_back(line.substr(start, tab - start));
        start = tab + 1;
    }
}

Result<State> parse_state(std::string_view text) {
    if (text == "handshake") {
        return State::handshake;
    }
    if (text == "status") {
        return State::status;
    }
    if (text == "login") {
        return State::login;
    }
    if (text == "configuration") {
        return State::configuration;
    }
    if (text == "play") {
        return State::play;
    }
    return Error{ErrorCode::malformed, "unknown connection state in manifest"};
}

Result<Bound> parse_bound(std::string_view text) {
    if (text == "clientbound") {
        return Bound::clientbound;
    }
    if (text == "serverbound") {
        return Bound::serverbound;
    }
    return Error{ErrorCode::malformed, "unknown direction in manifest"};
}

Result<std::int32_t> parse_id(std::string_view text) {
    if (text.empty()) {
        return Error{ErrorCode::malformed, "empty packet id"};
    }
    std::int64_t value = 0;
    for (char c : text) {
        if (c < '0' || c > '9') {
            return Error{ErrorCode::malformed, "non numeric packet id"};
        }
        value = value * 10 + (c - '0');
        if (value > 0x7FFFFFFF) {
            return Error{ErrorCode::value_out_of_range, "packet id out of range"};
        }
    }
    return static_cast<std::int32_t>(value);
}

}  // namespace

Status PacketTable::load_tsv(std::string_view content) {
    std::size_t position = 0;
    while (position < content.size()) {
        std::size_t end = content.find('\n', position);
        if (end == std::string_view::npos) {
            end = content.size();
        }
        std::string_view line = content.substr(position, end - position);
        position = end + 1;

        if (!line.empty() && line.back() == '\r') {
            line.remove_suffix(1);
        }
        if (line.empty() || line.front() == '#') {
            continue;
        }

        const auto fields = split_tabs(line);
        if (fields.size() < 4) {
            return Status::fail(ErrorCode::malformed, "packet line has fewer than 4 fields");
        }

        auto state = parse_state(fields[0]);
        if (!state) {
            return Status::fail(state.error().code, state.error().detail);
        }
        auto bound = parse_bound(fields[1]);
        if (!bound) {
            return Status::fail(bound.error().code, bound.error().detail);
        }
        auto id = parse_id(fields[3]);
        if (!id) {
            return Status::fail(id.error().code, id.error().detail);
        }

        auto status = add(PacketEntry{state.value(), bound.value(), std::string(fields[2]),
                                      id.value(), 0});
        if (!status) {
            return status;
        }
    }
    return Status::success();
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
