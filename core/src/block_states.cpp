#include "velnoria/world/block_states.hpp"

#include <limits>

namespace velnoria::world {
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

Result<std::uint32_t> parse_id(std::string_view text) {
    if (text.empty()) {
        return Error{ErrorCode::malformed, "empty block state id"};
    }
    std::uint64_t value = 0;
    for (char c : text) {
        if (c < '0' || c > '9') {
            return Error{ErrorCode::malformed, "non numeric block state id"};
        }
        value = value * 10 + static_cast<std::uint64_t>(c - '0');
        if (value > std::numeric_limits<std::uint32_t>::max()) {
            return Error{ErrorCode::value_out_of_range, "block state id out of range"};
        }
    }
    return static_cast<std::uint32_t>(value);
}

}  // namespace

Status BlockStateRegistry::load_tsv(std::string_view content) {
    entries_.clear();

    std::size_t position = 0;
    std::uint32_t expected_id = 0;

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
        if (fields.size() < 2) {
            return Status::fail(ErrorCode::malformed, "block state line has fewer than 2 fields");
        }

        auto id = parse_id(fields[0]);
        if (!id) {
            return Status::fail(id.error().code, id.error().detail);
        }
        if (id.value() != expected_id) {
            return Status::fail(ErrorCode::malformed,
                                "block state ids are not dense at " + std::to_string(expected_id));
        }

        BlockStateEntry entry;
        entry.id = id.value();
        entry.block = std::string(fields[1]);
        if (fields.size() > 2) {
            entry.properties = std::string(fields[2]);
        }
        entries_.push_back(std::move(entry));
        ++expected_id;
    }

    if (entries_.empty()) {
        return Status::fail(ErrorCode::malformed, "block state table is empty");
    }
    return Status::success();
}

const BlockStateEntry* BlockStateRegistry::find(std::uint32_t id) const {
    if (id >= entries_.size()) {
        return nullptr;
    }
    return &entries_[id];
}

}  // namespace velnoria::world
