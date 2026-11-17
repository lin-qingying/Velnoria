#include "lumenvox/core/md5.hpp"

#include <cstddef>

namespace lumenvox {
namespace {

// Round constants: floor(2^32 * abs(sin(i + 1))), as defined by RFC 1321.
constexpr std::uint32_t kK[64] = {
    0xd76aa478u, 0xe8c7b756u, 0x242070dbu, 0xc1bdceeeu, 0xf57c0fafu, 0x4787c62au,
    0xa8304613u, 0xfd469501u, 0x698098d8u, 0x8b44f7afu, 0xffff5bb1u, 0x895cd7beu,
    0x6b901122u, 0xfd987193u, 0xa679438eu, 0x49b40821u, 0xf61e2562u, 0xc040b340u,
    0x265e5a51u, 0xe9b6c7aau, 0xd62f105du, 0x02441453u, 0xd8a1e681u, 0xe7d3fbc8u,
    0x21e1cde6u, 0xc33707d6u, 0xf4d50d87u, 0x455a14edu, 0xa9e3e905u, 0xfcefa3f8u,
    0x676f02d9u, 0x8d2a4c8au, 0xfffa3942u, 0x8771f681u, 0x6d9d6122u, 0xfde5380cu,
    0xa4beea44u, 0x4bdecfa9u, 0xf6bb4b60u, 0xbebfbc70u, 0x289b7ec6u, 0xeaa127fau,
    0xd4ef3085u, 0x04881d05u, 0xd9d4d039u, 0xe6db99e5u, 0x1fa27cf8u, 0xc4ac5665u,
    0xf4292244u, 0x432aff97u, 0xab9423a7u, 0xfc93a039u, 0x655b59c3u, 0x8f0ccc92u,
    0xffeff47du, 0x85845dd1u, 0x6fa87e4fu, 0xfe2ce6e0u, 0xa3014314u, 0x4e0811a1u,
    0xf7537e82u, 0xbd3af235u, 0x2ad7d2bbu, 0xeb86d391u,
};

// Per-round left rotation amounts.
constexpr int kS[64] = {
    7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
    5, 9,  14, 20, 5, 9,  14, 20, 5, 9,  14, 20, 5, 9,  14, 20,
    4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
    6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21,
};

inline std::uint32_t rotl(std::uint32_t value, int count) {
    return (value << count) | (value >> (32 - count));
}

void process_block(const std::uint8_t* block, std::uint32_t& a, std::uint32_t& b, std::uint32_t& c,
                   std::uint32_t& d) {
    std::uint32_t words[16];
    for (int i = 0; i < 16; ++i) {
        words[i] = static_cast<std::uint32_t>(block[i * 4]) |
                   (static_cast<std::uint32_t>(block[i * 4 + 1]) << 8) |
                   (static_cast<std::uint32_t>(block[i * 4 + 2]) << 16) |
                   (static_cast<std::uint32_t>(block[i * 4 + 3]) << 24);
    }

    std::uint32_t aa = a;
    std::uint32_t bb = b;
    std::uint32_t cc = c;
    std::uint32_t dd = d;

    for (int i = 0; i < 64; ++i) {
        std::uint32_t f = 0;
        int g = 0;
        if (i < 16) {
            f = (bb & cc) | (~bb & dd);
            g = i;
        } else if (i < 32) {
            f = (dd & bb) | (~dd & cc);
            g = (5 * i + 1) % 16;
        } else if (i < 48) {
            f = bb ^ cc ^ dd;
            g = (3 * i + 5) % 16;
        } else {
            f = cc ^ (bb | ~dd);
            g = (7 * i) % 16;
        }

        const std::uint32_t temp = dd;
        dd = cc;
        cc = bb;
        bb = bb + rotl(aa + f + kK[i] + words[g], kS[i]);
        aa = temp;
    }

    a += aa;
    b += bb;
    c += cc;
    d += dd;
}

}  // namespace

Digest16 md5(std::span<const std::uint8_t> data) {
    std::uint32_t a = 0x67452301u;
    std::uint32_t b = 0xefcdab89u;
    std::uint32_t c = 0x98badcfeu;
    std::uint32_t d = 0x10325476u;

    const std::size_t full_blocks = data.size() / 64;
    for (std::size_t i = 0; i < full_blocks; ++i) {
        process_block(data.data() + i * 64, a, b, c, d);
    }

    // Tail: remaining bytes, 0x80 marker, zero padding, then the bit length as
    // a little endian 64 bit value.
    std::uint8_t tail[128] = {};
    const std::size_t remainder = data.size() - full_blocks * 64;
    for (std::size_t i = 0; i < remainder; ++i) {
        tail[i] = data[full_blocks * 64 + i];
    }
    tail[remainder] = 0x80u;

    const std::size_t tail_size = remainder + 1 <= 56 ? 64 : 128;
    const std::uint64_t bit_length = static_cast<std::uint64_t>(data.size()) * 8u;
    for (int i = 0; i < 8; ++i) {
        tail[tail_size - 8 + i] = static_cast<std::uint8_t>((bit_length >> (8 * i)) & 0xFFu);
    }

    process_block(tail, a, b, c, d);
    if (tail_size == 128) {
        process_block(tail + 64, a, b, c, d);
    }

    Digest16 digest{};
    const std::uint32_t words[4] = {a, b, c, d};
    for (int i = 0; i < 4; ++i) {
        digest[i * 4 + 0] = static_cast<std::uint8_t>(words[i] & 0xFFu);
        digest[i * 4 + 1] = static_cast<std::uint8_t>((words[i] >> 8) & 0xFFu);
        digest[i * 4 + 2] = static_cast<std::uint8_t>((words[i] >> 16) & 0xFFu);
        digest[i * 4 + 3] = static_cast<std::uint8_t>((words[i] >> 24) & 0xFFu);
    }
    return digest;
}

Digest16 md5(std::string_view data) {
    return md5(std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(data.data()),
                                             data.size()));
}

std::string to_hex(std::span<const std::uint8_t> bytes) {
    static constexpr char kDigits[] = "0123456789abcdef";
    std::string out;
    out.reserve(bytes.size() * 2);
    for (auto byte : bytes) {
        out.push_back(kDigits[byte >> 4]);
        out.push_back(kDigits[byte & 0x0Fu]);
    }
    return out;
}

}  // namespace lumenvox
