#include "velnoria/protocol/packets/handshake.hpp"

#include "velnoria/core/varint.hpp"
#include "velnoria/protocol/types.hpp"

namespace velnoria::protocol {

Status write_handshake(const Handshake& handshake, ByteWriter& writer) {
    if (handshake.protocol_version < 0) {
        return Status::fail(ErrorCode::value_out_of_range, "negative protocol version");
    }
    if (handshake.next_state != NextState::status && handshake.next_state != NextState::login) {
        return Status::fail(ErrorCode::value_out_of_range, "invalid next state");
    }

    write_varint(static_cast<std::uint32_t>(handshake.protocol_version), writer);

    auto status = write_string(handshake.server_address, writer, kMaxServerAddressUnits);
    if (!status) {
        return status;
    }

    status = write_u16(handshake.port, writer);
    if (!status) {
        return status;
    }

    write_varint(static_cast<std::uint32_t>(handshake.next_state), writer);
    return Status::success();
}

Result<Handshake> read_handshake(ByteReader& reader) {
    auto version = read_varint(reader);
    if (!version) {
        return version.error();
    }
    if (version.value() > 0x7FFFFFFFu) {
        return Error{ErrorCode::value_out_of_range, "protocol version out of range"};
    }

    auto address = read_string(reader, kMaxServerAddressUnits);
    if (!address) {
        return address.error();
    }

    auto port = read_u16(reader);
    if (!port) {
        return port.error();
    }

    auto next_state = read_varint(reader);
    if (!next_state) {
        return next_state.error();
    }
    if (next_state.value() != static_cast<std::uint32_t>(NextState::status) &&
        next_state.value() != static_cast<std::uint32_t>(NextState::login)) {
        return Error{ErrorCode::value_out_of_range, "invalid next state"};
    }

    Handshake handshake;
    handshake.protocol_version = static_cast<std::int32_t>(version.value());
    handshake.server_address = std::move(address).take();
    handshake.port = port.value();
    handshake.next_state = static_cast<NextState>(next_state.value());
    return handshake;
}

}  // namespace velnoria::protocol
