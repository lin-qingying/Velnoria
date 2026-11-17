#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "velnoria/core/byte_reader.hpp"
#include "velnoria/core/error.hpp"

// Wire framing: VarInt length prefix, then an optional zlib layer whose
// threshold is negotiated during Login.
//
// Compression is injected rather than linked so the core has no third party
// dependency at this layer; during Login the codec starts disabled and is
// upgraded once the server sends Set Compression.

namespace velnoria {

// Compression backend. Implementations must be deterministic: identical input
// yields identical output for a given level, so re-encoded frames can be
// compared byte for byte against captured ones (P0).
class Compressor {
public:
    virtual ~Compressor() = default;

    virtual Status compress(std::span<const std::uint8_t> input,
                            std::vector<std::uint8_t>& output) = 0;

    virtual Status decompress(std::span<const std::uint8_t> input,
                              std::size_t expected_size,
                              std::vector<std::uint8_t>& output) = 0;
};

struct FrameLimits {
    std::size_t max_frame_bytes = 2u * 1024u * 1024u;
    std::size_t max_uncompressed_bytes = 8u * 1024u * 1024u;
};

class FrameCodec {
public:
    FrameCodec() = default;

    // Called when the server sends Set Compression. A threshold of 0 disables
    // the layer entirely, matching vanilla semantics.
    void enable_compression(std::size_t threshold, Compressor* compressor);

    [[nodiscard]] bool compression_enabled() const noexcept { return compressor_ != nullptr; }
    [[nodiscard]] std::size_t threshold() const noexcept { return threshold_; }

    void set_limits(FrameLimits limits) { limits_ = limits; }

    // Consumes exactly one frame from reader. On success payload_out holds the
    // decompressed packet body (packet id + fields).
    Status decode(ByteReader& reader, std::vector<std::uint8_t>& payload_out);

    // Appends one framed packet to frame_out.
    Status encode(std::span<const std::uint8_t> payload, std::vector<std::uint8_t>& frame_out);

private:
    std::size_t threshold_ = 0;
    Compressor* compressor_ = nullptr;
    FrameLimits limits_{};
};

}  // namespace velnoria
