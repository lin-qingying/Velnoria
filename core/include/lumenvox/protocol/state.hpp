#pragma once

#include <cstdint>

namespace lumenvox::protocol {

// Connection states, in the order the handshake promotes through them.
enum class State : std::uint8_t {
    handshake = 0,
    status = 1,
    login = 2,
    configuration = 3,
    play = 4,
};

enum class Bound : std::uint8_t {
    clientbound = 0,
    serverbound = 1,
};

// Handshake "next state" field values.
enum class NextState : std::int32_t {
    status = 1,
    login = 2,
};

}  // namespace lumenvox::protocol
