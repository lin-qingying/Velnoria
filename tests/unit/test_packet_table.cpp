#include <cstdint>

#include "velnoria/protocol/packet_table.hpp"
#include "test_harness.hpp"

using velnoria::ErrorCode;
using velnoria::protocol::Bound;
using velnoria::protocol::PacketEntry;
using velnoria::protocol::PacketTable;
using velnoria::protocol::State;

namespace {

PacketEntry entry(State state, Bound bound, const char* name, std::int32_t id) {
    return PacketEntry{state, bound, name, id, 0};
}

}  // namespace

LV_TEST(packet_table_resolves_both_directions) {
    PacketTable table;
    LV_REQUIRE(table.add(entry(State::play, Bound::clientbound, "login", 0x2B)).ok());
    LV_REQUIRE(table.add(entry(State::play, Bound::clientbound, "keep_alive", 0x26)).ok());
    LV_REQUIRE(table.add(entry(State::play, Bound::serverbound, "keep_alive", 0x18)).ok());

    auto id = table.id_of(State::play, Bound::clientbound, "login");
    LV_REQUIRE(id.ok());
    LV_CHECK_EQ(id.value(), 0x2B);

    auto name = table.name_of(State::play, Bound::serverbound, 0x18);
    LV_REQUIRE(name.ok());
    LV_CHECK_EQ(std::string(name.value()), std::string("keep_alive"));

    LV_CHECK_EQ(table.size(), 3u);
}

LV_TEST(packet_table_separates_states_and_directions) {
    PacketTable table;
    LV_REQUIRE(table.add(entry(State::play, Bound::clientbound, "keep_alive", 0x26)).ok());

    // Same name in another state must not resolve: a state machine bug that
    // reads the wrong table would otherwise pass silently.
    auto wrong_state = table.id_of(State::configuration, Bound::clientbound, "keep_alive");
    LV_CHECK_ERR(wrong_state, ErrorCode::unsupported);

    auto wrong_bound = table.id_of(State::play, Bound::serverbound, "keep_alive");
    LV_CHECK_ERR(wrong_bound, ErrorCode::unsupported);
}

LV_TEST(packet_table_rejects_duplicates) {
    PacketTable table;
    LV_REQUIRE(table.add(entry(State::play, Bound::clientbound, "login", 0x2B)).ok());

    auto duplicate_name = table.add(entry(State::play, Bound::clientbound, "login", 0x2C));
    LV_CHECK_ERR(duplicate_name, ErrorCode::malformed);

    auto duplicate_id = table.add(entry(State::play, Bound::clientbound, "respawn", 0x2B));
    LV_CHECK_ERR(duplicate_id, ErrorCode::malformed);
}

LV_TEST(packet_table_rejects_negative_id) {
    PacketTable table;
    LV_CHECK_ERR(table.add(entry(State::play, Bound::clientbound, "bad", -1)),
                 ErrorCode::value_out_of_range);
}

LV_TEST(packet_table_hash_is_insertion_order_independent) {
    PacketTable first;
    first.add(entry(State::play, Bound::clientbound, "a", 1));
    first.add(entry(State::play, Bound::clientbound, "b", 2));

    PacketTable second;
    second.add(entry(State::play, Bound::clientbound, "b", 2));
    second.add(entry(State::play, Bound::clientbound, "a", 1));

    LV_CHECK_EQ(first.content_hash(), second.content_hash());

    PacketTable third;
    third.add(entry(State::play, Bound::clientbound, "a", 1));
    third.add(entry(State::play, Bound::clientbound, "b", 3));
    LV_REQUIRE(first.content_hash() != third.content_hash());
}
