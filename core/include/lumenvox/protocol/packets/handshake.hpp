#pragma once

#include <cstdint>
#include <string>

#include "lumenvox/core/byte_reader.hpp"
#include "lumenvox/core/byte_writer.hpp"
#include "lumenvox/core/error.hpp"
#include "lumenvox/protocol/state.hpp"

// Handshake (a.k.a. "intention") packet body.
//
// The packet id is not written here on purpose: ids come from the manifest that
// prepare generates, so the session layer prepends it from PacketTable. Writing
// a literal 0x00 here would silently pin the code to one protocol revision.

namespace lumenvox::protocol {

inline constexpr std::size_t kMaxServerAddressUnits = 255;

struct Handshake {
    std::int32_t protocol_version = 0;
    std::string server_address;
    std::uint16_t port = 25565;
    NextState next_state = NextState::status;
};

[[nodiscard]] Status write_handshake(const Handshake& handshake, ByteWriter& writer);
[[nodiscard]] Result<Handshake> read_handshake(ByteReader& reader);

}  // namespace lumenvox::protocol
