#include <cstdint>
#include <vector>

#include "lumenvox/core/byte_reader.hpp"
#include "lumenvox/core/byte_writer.hpp"
#include "lumenvox/core/varint.hpp"
#include "lumenvox/protocol/frame.hpp"
#include "test_harness.hpp"

using lumenvox::ByteReader;
using lumenvox::ErrorCode;
using lumenvox::FrameCodec;
using lumenvox::FrameLimits;
using lumenvox::Status;

namespace {

// Deliberately naive run length codec. It exists to exercise both frame paths
// (compressed and under threshold) without pulling zlib into the core library.
// Format: repeated (count, value) byte pairs, count in 1..255.
class ZeroRleCompressor final : public lumenvox::Compressor {
public:
    Status compress(std::span<const std::uint8_t> input,
                    std::vector<std::uint8_t>& output) override {
        std::size_t index = 0;
        while (index < input.size()) {
            const std::uint8_t value = input[index];
            std::size_t run = 1;
            while (index + run < input.size() && input[index + run] == value && run < 255) {
                ++run;
            }
            output.push_back(static_cast<std::uint8_t>(run));
            output.push_back(value);
            index += run;
        }
        return Status::success();
    }

    Status decompress(std::span<const std::uint8_t> input, std::size_t expected_size,
                      std::vector<std::uint8_t>& output) override {
        std::size_t index = 0;
        while (index + 1 < input.size()) {
            const std::size_t run = input[index];
            const std::uint8_t value = input[index + 1];
            if (output.size() + run > expected_size) {
                return Status::fail(ErrorCode::compression_failed, "run exceeds declared size");
            }
            output.insert(output.end(), run, value);
            index += 2;
        }
        if (output.size() != expected_size) {
            return Status::fail(ErrorCode::compression_failed, "decompressed size mismatch");
        }
        return Status::success();
    }
};

std::vector<std::uint8_t> decode_roundtrip(FrameCodec& codec, const std::vector<std::uint8_t>& frame) {
    ByteReader reader(frame);
    std::vector<std::uint8_t> payload;
    auto status = codec.decode(reader, payload);
    LV_REQUIRE(status.ok());
    LV_REQUIRE(reader.empty());
    return payload;
}

}  // namespace

LV_TEST(frame_roundtrip_without_compression) {
    FrameCodec codec;
    const std::vector<std::uint8_t> payload{0x0A, 0x00, 0x01, 0x02, 0xFF};

    std::vector<std::uint8_t> frame;
    LV_REQUIRE(codec.encode(payload, frame).ok());
    LV_CHECK_EQ(frame.size(), payload.size() + 1);  // single byte length prefix

    const auto decoded = decode_roundtrip(codec, frame);
    LV_CHECK_EQ(decoded, payload);
}

LV_TEST(frame_decode_rejects_truncated_body) {
    FrameCodec codec;
    const std::vector<std::uint8_t> payload{0x01, 0x02, 0x03};

    std::vector<std::uint8_t> frame;
    LV_REQUIRE(codec.encode(payload, frame).ok());
    frame.pop_back();

    ByteReader reader(frame);
    std::vector<std::uint8_t> out;
    LV_CHECK_ERR(codec.decode(reader, out), ErrorCode::truncated);
}

LV_TEST(frame_encode_enforces_size_limit) {
    FrameCodec codec;
    FrameLimits limits;
    limits.max_frame_bytes = 4;
    codec.set_limits(limits);

    const std::vector<std::uint8_t> payload(64, 0x11);
    std::vector<std::uint8_t> frame;
    LV_CHECK_ERR(codec.encode(payload, frame), ErrorCode::frame_too_large);
}

LV_TEST(frame_small_payload_bypasses_compression) {
    FrameCodec codec;
    ZeroRleCompressor compressor;
    codec.enable_compression(64, &compressor);

    // Below threshold: single 0x00 data-length prefix, then raw payload.
    const std::vector<std::uint8_t> payload{0x00, 0x0A, 0x0B};
    std::vector<std::uint8_t> frame;
    LV_REQUIRE(codec.encode(payload, frame).ok());

    const auto decoded = decode_roundtrip(codec, frame);
    LV_CHECK_EQ(decoded, payload);
}

LV_TEST(frame_large_payload_uses_compression) {
    FrameCodec codec;
    ZeroRleCompressor compressor;
    codec.enable_compression(64, &compressor);

    const std::vector<std::uint8_t> payload(1024, 0x00);
    std::vector<std::uint8_t> frame;
    LV_REQUIRE(codec.encode(payload, frame).ok());

    // 1024 zero bytes compress to a handful of pairs, well under the wire size.
    LV_REQUIRE(frame.size() < payload.size() / 2);

    const auto decoded = decode_roundtrip(codec, frame);
    LV_CHECK_EQ(decoded, payload);
}

LV_TEST(frame_rejects_compressed_below_threshold) {
    FrameCodec codec;
    ZeroRleCompressor compressor;
    codec.enable_compression(256, &compressor);

    // Hand build a frame claiming a 1 byte payload, below the negotiated
    // threshold of 256. Vanilla rejects this, and so do we.
    std::vector<std::uint8_t> body;
    lumenvox::ByteWriter writer(body);
    lumenvox::write_varint(1u, writer);
    writer.write_u8(0x7A);

    std::vector<std::uint8_t> frame;
    lumenvox::ByteWriter frame_writer(frame);
    lumenvox::write_varint(static_cast<std::uint32_t>(body.size()), frame_writer);
    frame_writer.write_bytes(body);

    ByteReader reader(frame);
    std::vector<std::uint8_t> out;
    LV_CHECK_ERR(codec.decode(reader, out), ErrorCode::malformed);
}

LV_TEST(frame_rejects_oversized_decompressed_size) {
    FrameCodec codec;
    ZeroRleCompressor compressor;
    codec.enable_compression(64, &compressor);

    FrameLimits limits;
    limits.max_uncompressed_bytes = 128;
    codec.set_limits(limits);

    std::vector<std::uint8_t> body;
    lumenvox::ByteWriter writer(body);
    lumenvox::write_varint(4096u, writer);
    writer.write_u8(0xFF);

    std::vector<std::uint8_t> frame;
    lumenvox::ByteWriter frame_writer(frame);
    lumenvox::write_varint(static_cast<std::uint32_t>(body.size()), frame_writer);
    frame_writer.write_bytes(body);

    ByteReader reader(frame);
    std::vector<std::uint8_t> out;
    LV_CHECK_ERR(codec.decode(reader, out), ErrorCode::frame_too_large);
}
