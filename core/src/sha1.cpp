#include "velnoria/core/sha1.hpp"

#include <algorithm>

namespace velnoria {
namespace {

inline std::uint32_t rotl(std::uint32_t value, int count) {
    return (value << count) | (value >> (32 - count));
}

}  // namespace

void Sha1::process_block(const std::uint8_t* block) {
    std::uint32_t w[80];
    for (int i = 0; i < 16; ++i) {
        w[i] = (static_cast<std::uint32_t>(block[i * 4]) << 24) |
               (static_cast<std::uint32_t>(block[i * 4 + 1]) << 16) |
               (static_cast<std::uint32_t>(block[i * 4 + 2]) << 8) |
               static_cast<std::uint32_t>(block[i * 4 + 3]);
    }
    for (int i = 16; i < 80; ++i) {
        w[i] = rotl(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
    }

    std::uint32_t a = state_[0];
    std::uint32_t b = state_[1];
    std::uint32_t c = state_[2];
    std::uint32_t d = state_[3];
    std::uint32_t e = state_[4];

    for (int i = 0; i < 80; ++i) {
        std::uint32_t f = 0;
        std::uint32_t k = 0;
        if (i < 20) {
            f = (b & c) | (~b & d);
            k = 0x5A827999u;
        } else if (i < 40) {
            f = b ^ c ^ d;
            k = 0x6ED9EBA1u;
        } else if (i < 60) {
            f = (b & c) | (b & d) | (c & d);
            k = 0x8F1BBCDCu;
        } else {
            f = b ^ c ^ d;
            k = 0xCA62C1D6u;
        }
        const std::uint32_t temp = rotl(a, 5) + f + e + k + w[i];
        e = d;
        d = c;
        c = rotl(b, 30);
        b = a;
        a = temp;
    }

    state_[0] += a;
    state_[1] += b;
    state_[2] += c;
    state_[3] += d;
    state_[4] += e;
}

void Sha1::update(std::span<const std::uint8_t> data) {
    total_bytes_ += data.size();
    std::size_t offset = 0;

    if (buffer_size_ > 0) {
        const std::size_t take = std::min<std::size_t>(64 - buffer_size_, data.size());
        for (std::size_t i = 0; i < take; ++i) {
            buffer_[buffer_size_ + i] = data[i];
        }
        buffer_size_ += take;
        offset = take;
        if (buffer_size_ == 64) {
            process_block(buffer_);
            buffer_size_ = 0;
        }
    }

    while (data.size() - offset >= 64) {
        process_block(data.data() + offset);
        offset += 64;
    }

    for (std::size_t i = offset; i < data.size(); ++i) {
        buffer_[buffer_size_++] = data[i];
    }
}

Digest20 Sha1::final() {
    if (!finished_) {
        const std::uint64_t bit_length = total_bytes_ * 8u;
        const std::uint8_t marker = 0x80u;
        update(std::span<const std::uint8_t>(&marker, 1));

        const std::uint8_t zero = 0;
        while (buffer_size_ != 56) {
            update(std::span<const std::uint8_t>(&zero, 1));
        }

        std::uint8_t length_bytes[8] = {};
        for (int i = 0; i < 8; ++i) {
            length_bytes[i] = static_cast<std::uint8_t>((bit_length >> (56 - 8 * i)) & 0xFFu);
        }
        update(std::span<const std::uint8_t>(length_bytes, 8));
        finished_ = true;
    }

    Digest20 digest{};
    for (int i = 0; i < 5; ++i) {
        digest[i * 4 + 0] = static_cast<std::uint8_t>((state_[i] >> 24) & 0xFFu);
        digest[i * 4 + 1] = static_cast<std::uint8_t>((state_[i] >> 16) & 0xFFu);
        digest[i * 4 + 2] = static_cast<std::uint8_t>((state_[i] >> 8) & 0xFFu);
        digest[i * 4 + 3] = static_cast<std::uint8_t>(state_[i] & 0xFFu);
    }
    return digest;
}

Digest20 sha1(std::span<const std::uint8_t> data) {
    Sha1 hasher;
    hasher.update(data);
    return hasher.final();
}

Digest20 sha1(std::string_view data) {
    return sha1(std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(data.data()),
                                              data.size()));
}

}  // namespace velnoria
