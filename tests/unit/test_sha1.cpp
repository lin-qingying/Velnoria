#include <cstdint>
#include <string>
#include <vector>

#include "velnoria/core/sha1.hpp"
#include "test_harness.hpp"

using velnoria::Digest20;
using velnoria::Sha1;

namespace {

std::string hex(const Digest20& digest) {
    static constexpr char kDigits[] = "0123456789abcdef";
    std::string out;
    for (auto byte : digest) {
        out.push_back(kDigits[byte >> 4]);
        out.push_back(kDigits[byte & 0x0Fu]);
    }
    return out;
}

}  // namespace

LV_TEST(sha1_known_vectors) {
    LV_CHECK_EQ(hex(velnoria::sha1(std::string_view(""))) ,
                std::string("da39a3ee5e6b4b0d3255bfef95601890afd80709"));
    LV_CHECK_EQ(hex(velnoria::sha1(std::string_view("abc"))),
                std::string("a9993e364706816aba3e25717850c26c9cd0d89d"));
    LV_CHECK_EQ(hex(velnoria::sha1(std::string_view("The quick brown fox jumps over the lazy dog"))),
                std::string("2fd4e1c67a2d28fced849ee1bb76e7391b93eb12"));
}

LV_TEST(sha1_streaming_matches_one_shot) {
    std::vector<std::uint8_t> data;
    data.reserve(100000);
    for (std::size_t i = 0; i < 100000; ++i) {
        data.push_back(static_cast<std::uint8_t>(i * 31u + 7u));
    }

    const auto one_shot = velnoria::sha1(data);

    for (std::size_t chunk : {1u, 7u, 63u, 64u, 65u, 4096u}) {
        Sha1 hasher;
        std::size_t offset = 0;
        while (offset < data.size()) {
            const std::size_t take = std::min(chunk, data.size() - offset);
            hasher.update(std::span<const std::uint8_t>(data.data() + offset, take));
            offset += take;
        }
        LV_REQUIRE(hasher.final() == one_shot);
    }
}

LV_TEST(sha1_padding_boundaries) {
    // 55, 56, 63 and 64 bytes exercise every tail path, including the case that
    // needs an extra block.
    const std::size_t sizes[] = {54, 55, 56, 57, 63, 64, 65, 119, 120, 127, 128};
    std::string previous;
    for (auto size : sizes) {
        const std::string input(size, 'a');
        const auto digest = hex(velnoria::sha1(std::string_view(input)));
        LV_REQUIRE(digest.size() == 40);
        LV_REQUIRE(digest != previous);
        previous = digest;
    }
}
