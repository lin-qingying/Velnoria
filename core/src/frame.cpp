#include "lumenvox/protocol/frame.hpp"

#include <string>

#include "lumenvox/core/varint.hpp"

namespace lumenvox {

void FrameCodec::enable_compression(std::size_t threshold, Compressor* compressor) {
    if (threshold == 0 || compressor == nullptr) {
        threshold_ = 0;
        compressor_ = nullptr;
        return;
    }
    threshold_ = threshold;
    compressor_ = compressor;
}

Status FrameCodec::decode(ByteReader& reader, std::vector<std::uint8_t>& payload_out) {
    auto length = read_varint(reader);
    if (!length) {
        return Status::fail(length.error().code, length.error().detail);
    }
    const auto frame_len = static_cast<std::size_t>(length.value());
    if (frame_len > limits_.max_frame_bytes) {
        return Status::fail(ErrorCode::frame_too_large,
                            "frame length " + std::to_string(frame_len) + " exceeds limit");
    }

    auto body = reader.read_bytes(frame_len);
    if (!body) {
        return Status::fail(body.error().code, body.error().detail);
    }

    if (compressor_ == nullptr) {
        payload_out.assign(body.value().begin(), body.value().end());
        return Status::success();
    }

    ByteReader body_reader(body.value());
    auto data_length = read_varint(body_reader);
    if (!data_length) {
        return Status::fail(data_length.error().code, data_length.error().detail);
    }

    const auto declared = static_cast<std::size_t>(data_length.value());
    if (declared == 0) {
        // Below threshold: body follows uncompressed.
        const auto raw = body_reader.tail();
        payload_out.assign(raw.begin(), raw.end());
        return Status::success();
    }

    if (declared > limits_.max_uncompressed_bytes) {
        return Status::fail(ErrorCode::frame_too_large,
                            "declared uncompressed size " + std::to_string(declared) +
                                " exceeds limit");
    }
    if (declared < threshold_) {
        // Vanilla rejects this too; accepting it would mask a tampered stream.
        return Status::fail(ErrorCode::malformed,
                            "compressed frame below negotiated threshold");
    }

    return compressor_->decompress(body_reader.tail(), declared, payload_out);
}

Status FrameCodec::encode(std::span<const std::uint8_t> payload, std::vector<std::uint8_t>& frame_out) {
    std::vector<std::uint8_t> body;

    if (compressor_ != nullptr) {
        if (payload.size() >= threshold_) {
            std::vector<std::uint8_t> compressed;
            auto status = compressor_->compress(payload, compressed);
            if (!status) {
                return status;
            }
            if (compressed.size() < payload.size()) {
                body.reserve(kMaxVarIntBytes + compressed.size());
                ByteWriter writer(body);
                write_varint(static_cast<std::uint32_t>(payload.size()), writer);
                writer.write_bytes(compressed);
            } else {
                body.reserve(kMaxVarIntBytes + payload.size());
                ByteWriter writer(body);
                write_varint(0u, writer);
                writer.write_bytes(payload);
            }
        } else {
            body.reserve(kMaxVarIntBytes + payload.size());
            ByteWriter writer(body);
            write_varint(0u, writer);
            writer.write_bytes(payload);
        }
    } else {
        body.assign(payload.begin(), payload.end());
    }

    if (body.size() > limits_.max_frame_bytes) {
        return Status::fail(ErrorCode::frame_too_large, "encoded frame exceeds limit");
    }

    ByteWriter writer(frame_out);
    write_varint(static_cast<std::uint32_t>(body.size()), writer);
    writer.write_bytes(body);
    return Status::success();
}

}  // namespace lumenvox
