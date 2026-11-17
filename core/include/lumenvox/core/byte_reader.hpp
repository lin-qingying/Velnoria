#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

#include "lumenvox/core/error.hpp"

namespace lumenvox {

// Bounds checked forward cursor over a borrowed buffer. Every read either
// succeeds or returns an Error, so protocol decoding cannot read past the end
// of a frame (see docs/parity/levels.md, P0).
class ByteReader {
public:
    explicit ByteReader(std::span<const std::uint8_t> data) : data_(data) {}

    [[nodiscard]] std::size_t position() const noexcept { return pos_; }
    [[nodiscard]] std::size_t remaining() const noexcept { return data_.size() - pos_; }
    [[nodiscard]] bool empty() const noexcept { return remaining() == 0; }
    [[nodiscard]] std::span<const std::uint8_t> tail() const noexcept {
        return data_.subspan(pos_);
    }

    Result<std::uint8_t> read_u8();
    Result<std::span<const std::uint8_t>> read_bytes(std::size_t count);

    // Skips without reading; used for "unknown trailing payload" preservation.
    Status skip(std::size_t count);

private:
    std::span<const std::uint8_t> data_;
    std::size_t pos_ = 0;
};

}  // namespace lumenvox
