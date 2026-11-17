#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include "velnoria/protocol/packet_table.hpp"
#include "velnoria/world/block_states.hpp"
#include "test_harness.hpp"

using velnoria::ErrorCode;
using velnoria::protocol::Bound;
using velnoria::protocol::PacketTable;
using velnoria::protocol::State;
using velnoria::world::BlockStateRegistry;

namespace {

std::string read_file(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    std::ostringstream buffer;
    buffer << stream.rdbuf();
    return buffer.str();
}

}  // namespace

LV_TEST(packet_table_loads_tsv_manifest) {
    const std::string content =
        "# velnoria packets v1\n"
        "handshake\tserverbound\tminecraft:intention\t0\n"
        "status\tclientbound\tminecraft:status_response\t0\n"
        "play\tclientbound\tminecraft:block_update\t9\n";

    PacketTable table;
    LV_REQUIRE(table.load_tsv(content).ok());
    LV_CHECK_EQ(table.size(), 3u);

    auto id = table.id_of(State::play, Bound::clientbound, "minecraft:block_update");
    LV_REQUIRE(id.ok());
    LV_CHECK_EQ(id.value(), 9);

    auto name = table.name_of(State::handshake, Bound::serverbound, 0);
    LV_REQUIRE(name.ok());
    LV_CHECK_EQ(std::string(name.value()), std::string("minecraft:intention"));
}

LV_TEST(packet_table_tsv_rejects_unknown_state_and_duplicates) {
    PacketTable table;
    LV_CHECK_ERR(table.load_tsv("bogus\tclientbound\tx:y\t1\n"), ErrorCode::malformed);

    const std::string duplicated =
        "play\tclientbound\tminecraft:a\t1\n"
        "play\tclientbound\tminecraft:a\t2\n";
    LV_CHECK_ERR(table.load_tsv(duplicated), ErrorCode::malformed);
}

LV_TEST(block_states_load_tsv_requires_dense_ids) {
    const std::string content =
        "# velnoria block_states v1\n"
        "0\tminecraft:air\t\n"
        "1\tminecraft:stone\t\n"
        "2\tminecraft:oak_log\taxis=y\n";

    BlockStateRegistry registry;
    LV_REQUIRE(registry.load_tsv(content).ok());
    LV_CHECK_EQ(registry.size(), 3u);

    const auto* entry = registry.find(2);
    LV_REQUIRE(entry != nullptr);
    LV_CHECK_EQ(entry->block, std::string("minecraft:oak_log"));
    LV_CHECK_EQ(entry->properties, std::string("axis=y"));
    LV_REQUIRE(registry.find(9) == nullptr);
}

LV_TEST(block_states_reject_holes) {
    BlockStateRegistry registry;
    LV_CHECK_ERR(registry.load_tsv("# header\n0\tminecraft:air\t\n2\tminecraft:stone\t\n"),
                 ErrorCode::malformed);
    LV_CHECK_ERR(registry.load_tsv("# header\n"), ErrorCode::malformed);
}

// Optional integration check: runs only when a real export exists locally.
LV_TEST(prepared_export_is_self_consistent) {
    const std::filesystem::path packets_path =
        std::filesystem::path(VELNORIA_DATA_DIR) / "26.3-Fabric" / "packets.tsv";
    const std::filesystem::path states_path =
        std::filesystem::path(VELNORIA_DATA_DIR) / "26.3-Fabric" / "block_states.tsv";

    if (!std::filesystem::exists(packets_path) || !std::filesystem::exists(states_path)) {
        return;  // no local export yet
    }

    PacketTable table;
    LV_REQUIRE(table.load_tsv(read_file(packets_path)).ok());
    LV_REQUIRE(table.size() > 200);

    BlockStateRegistry registry;
    LV_REQUIRE(registry.load_tsv(read_file(states_path)).ok());
    LV_REQUIRE(registry.size() > 20000);

    const auto* air = registry.find(0);
    LV_REQUIRE(air != nullptr);
    LV_CHECK_EQ(air->block, std::string("minecraft:air"));
}
