#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "lumenvox/core/byte_reader.hpp"
#include "lumenvox/core/byte_writer.hpp"
#include "lumenvox/core/uuid.hpp"
#include "lumenvox/protocol/packets/handshake.hpp"
#include "lumenvox/protocol/packet_table.hpp"
#include "lumenvox/protocol/types.hpp"
#include "test_harness.hpp"

using lumenvox::ByteReader;
using lumenvox::ByteWriter;
using lumenvox::ErrorCode;
using lumenvox::protocol::BlockPos;
using lumenvox::protocol::Bound;
using lumenvox::protocol::Handshake;
using lumenvox::protocol::kMaxStringUnits;
using lumenvox::protocol::NextState;
using lumenvox::protocol::PacketEntry;
using lumenvox::protocol::PacketTable;
using lumenvox::protocol::State;

namespace {

using Bytes = std::vector<std::uint8_t>;

Bytes write_string_bytes(const std::string& value, std::size_t max_units = kMaxStringUnits) {
    Bytes bytes;
    ByteWriter writer(bytes);
    LV_REQUIRE(lumenvox::protocol::write_string(value, writer, max_units).ok());
    return bytes;
}

}  // namespace

LV_TEST(protocol_string_roundtrip_with_multibyte) {
    const std::string value = "h\xC3\xA9llo";  // 6 bytes, 5 code units
    const auto bytes = write_string_bytes(value);
    LV_REQUIRE(bytes.size() == 7);
    LV_CHECK_EQ(bytes[0], static_cast<std::uint8_t>(6));

    ByteReader reader(bytes);
    auto decoded = lumenvox::protocol::read_string(reader);
    LV_REQUIRE(decoded.ok());
    LV_CHECK_EQ(decoded.value(), value);
    LV_REQUIRE(reader.empty());
}

LV_TEST(protocol_string_empty_has_zero_prefix) {
    const auto bytes = write_string_bytes("");
    LV_REQUIRE(bytes.size() == 1);
    LV_CHECK_EQ(bytes[0], static_cast<std::uint8_t>(0));

    ByteReader reader(bytes);
    auto decoded = lumenvox::protocol::read_string(reader);
    LV_REQUIRE(decoded.ok());
    LV_REQUIRE(decoded.value().empty());
}

LV_TEST(protocol_string_rejects_invalid_utf8) {
    const Bytes bytes{0x02, 0xC3, 0x28};
    ByteReader reader(bytes);
    LV_CHECK_ERR(lumenvox::protocol::read_string(reader), ErrorCode::malformed);
}

LV_TEST(protocol_string_rejects_oversized_byte_prefix) {
    // Four code units bound the payload at 16 bytes; the prefix claims 17.
    const Bytes bytes{0x11};
    ByteReader reader(bytes);
    LV_CHECK_ERR(lumenvox::protocol::read_string(reader, 4), ErrorCode::value_out_of_range);
}

LV_TEST(protocol_string_rejects_too_many_units) {
    const auto bytes = write_string_bytes("abc");
    ByteReader reader(bytes);
    LV_CHECK_ERR(lumenvox::protocol::read_string(reader, 2), ErrorCode::value_out_of_range);
}

LV_TEST(protocol_string_write_rejects_invalid_utf8) {
    Bytes bytes;
    ByteWriter writer(bytes);
    LV_CHECK_ERR(lumenvox::protocol::write_string(std::string_view("\xC3"), writer),
                 ErrorCode::malformed);
}

LV_TEST(position_packs_to_expected_value) {
    Bytes bytes;
    ByteWriter writer(bytes);
    LV_REQUIRE(lumenvox::protocol::write_position(BlockPos{1, 2, 3}, writer).ok());

    // x << 38 | z << 12 | y == 0x00000040_00003002
    const Bytes expected{0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x30, 0x02};
    LV_CHECK_EQ(bytes, expected);
}

LV_TEST(position_roundtrips_negative_components) {
    const BlockPos cases[] = {BlockPos{0, 0, 0},
                              BlockPos{-1, -1, -1},
                              BlockPos{-1000, -64, 2000},
                              BlockPos{33554431, 2047, -33554432},
                              BlockPos{-33554432, -2048, 33554431}};

    for (const auto& pos : cases) {
        Bytes bytes;
        ByteWriter writer(bytes);
        LV_REQUIRE(lumenvox::protocol::write_position(pos, writer).ok());

        ByteReader reader(bytes);
        auto decoded = lumenvox::protocol::read_position(reader);
        LV_REQUIRE(decoded.ok());
        LV_REQUIRE(decoded.value() == pos);
    }
}

LV_TEST(position_truncates_out_of_range_components) {
    // 1 << 26 wraps to zero in a 26 bit field; 4096 wraps to zero in 12 bits.
    Bytes bytes;
    ByteWriter writer(bytes);
    LV_REQUIRE(lumenvox::protocol::write_position(BlockPos{1 << 26, 4096, 0}, writer).ok());

    ByteReader reader(bytes);
    auto decoded = lumenvox::protocol::read_position(reader);
    LV_REQUIRE(decoded.ok());
    LV_REQUIRE(decoded.value().x == 0 && decoded.value().y == 0 && decoded.value().z == 0);
}

LV_TEST(uuid_roundtrips_through_wire_form) {
    auto uuid = lumenvox::Uuid::parse("b50ad385-829d-3141-a216-7e7d7539ba7f");
    LV_REQUIRE(uuid.ok());

    Bytes bytes;
    ByteWriter writer(bytes);
    LV_REQUIRE(lumenvox::protocol::write_uuid(uuid.value(), writer).ok());

    const Bytes expected{0xB5, 0x0A, 0xD3, 0x85, 0x82, 0x9D, 0x31, 0x41,
                         0xA2, 0x16, 0x7E, 0x7D, 0x75, 0x39, 0xBA, 0x7F};
    LV_CHECK_EQ(bytes, expected);

    ByteReader reader(bytes);
    auto decoded = lumenvox::protocol::read_uuid(reader);
    LV_REQUIRE(decoded.ok());
    LV_REQUIRE(decoded.value() == uuid.value());
}

LV_TEST(handshake_body_encodes_expected_bytes) {
    Handshake handshake;
    handshake.protocol_version = 763;
    handshake.server_address = "localhost";
    handshake.port = 25565;
    handshake.next_state = NextState::login;

    Bytes bytes;
    ByteWriter writer(bytes);
    LV_REQUIRE(lumenvox::protocol::write_handshake(handshake, writer).ok());

    const Bytes expected{0xFB, 0x05, 0x09, 'l', 'o', 'c', 'a', 'l', 'h', 'o', 's', 't',
                         0x63, 0xDD, 0x02};
    LV_CHECK_EQ(bytes, expected);

    ByteReader reader(bytes);
    auto decoded = lumenvox::protocol::read_handshake(reader);
    LV_REQUIRE(decoded.ok());
    LV_REQUIRE(decoded.value().protocol_version == 763);
    LV_REQUIRE(decoded.value().server_address == std::string("localhost"));
    LV_REQUIRE(decoded.value().port == 25565);
    LV_REQUIRE(decoded.value().next_state == NextState::login);
}

LV_TEST(handshake_rejects_invalid_next_state) {
    // Same body as above with next state 3.
    const Bytes bytes{0xFB, 0x05, 0x09, 'l', 'o', 'c', 'a', 'l', 'h', 'o', 's', 't',
                      0x63, 0xDD, 0x03};
    ByteReader reader(bytes);
    LV_CHECK_ERR(lumenvox::protocol::read_handshake(reader), ErrorCode::value_out_of_range);
}

LV_TEST(handshake_packet_id_comes_from_the_table) {
    // The production code never writes a literal id; the session layer asks the
    // manifest. This test pins that contract for the handshake.
    PacketTable table;
    LV_REQUIRE(table.add(PacketEntry{State::handshake, Bound::serverbound, "intention", 0x00, 0})
                   .ok());

    auto id = table.id_of(State::handshake, Bound::serverbound, "intention");
    LV_REQUIRE(id.ok());

    Handshake handshake;
    handshake.protocol_version = 763;
    handshake.server_address = "localhost";
    handshake.port = 25565;
    handshake.next_state = NextState::status;

    Bytes body;
    ByteWriter body_writer(body);
    LV_REQUIRE(lumenvox::protocol::write_handshake(handshake, body_writer).ok());

    // Compose the packet: id then body. Packet framing adds the length prefix.
    Bytes packet;
    ByteWriter packet_writer(packet);
    packet_writer.write_u8(static_cast<std::uint8_t>(id.value()));
    packet_writer.write_bytes(body);

    LV_CHECK_EQ(packet.front(), static_cast<std::uint8_t>(0x00));
    LV_CHECK_EQ(packet.back(), static_cast<std::uint8_t>(static_cast<int>(NextState::status)));
}
