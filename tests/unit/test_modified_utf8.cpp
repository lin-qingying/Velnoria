#include <cstdint>
#include <utility>
#include <string>
#include <vector>

#include "velnoria/core/modified_utf8.hpp"
#include "test_harness.hpp"

using velnoria::ErrorCode;
using velnoria::text::decode_modified_utf8;
using velnoria::text::encode_modified_utf8;
using velnoria::text::modified_utf8_unit_count;

namespace {

std::vector<std::uint8_t> encode(const std::string& value) {
    std::vector<std::uint8_t> out;
    auto status = encode_modified_utf8(value, out);
    LV_REQUIRE(status.ok());
    return out;
}

std::string decode(const std::vector<std::uint8_t>& bytes) {
    auto result = decode_modified_utf8(bytes);
    LV_REQUIRE(result.ok());
    return std::move(result).take();
}

}  // namespace

LV_TEST(modified_utf8_passes_ascii_through) {
    const std::string input = "hello world";
    const auto bytes = encode(input);
    LV_CHECK_EQ(bytes.size(), input.size());
    LV_CHECK_EQ(decode(bytes), input);
}

LV_TEST(modified_utf8_encodes_nul_as_two_bytes) {
    const std::string input("a\0b", 3);
    const auto bytes = encode(input);
    const std::vector<std::uint8_t> expected{0x61, 0xC0, 0x80, 0x62};
    LV_CHECK_EQ(bytes, expected);
    LV_CHECK_EQ(decode(bytes), input);
    LV_CHECK_EQ(modified_utf8_unit_count(input), 3u);
}

LV_TEST(modified_utf8_encodes_astral_as_surrogate_pair) {
    // U+1F600, four byte in standard UTF-8.
    const std::string input = "\xF0\x9F\x98\x80";
    const auto bytes = encode(input);
    const std::vector<std::uint8_t> expected{0xED, 0xA0, 0xBD, 0xED, 0xB8, 0x80};
    LV_CHECK_EQ(bytes, expected);
    LV_CHECK_EQ(decode(bytes), input);
    LV_CHECK_EQ(modified_utf8_unit_count(input), 2u);
}

LV_TEST(modified_utf8_roundtrips_two_and_three_byte_forms) {
    // U+00E9 (2 byte), U+4E2D (3 byte), plus an astral character in one string.
    const std::string input = "\xC3\xA9\xE4\xB8\xAD\xF0\x9F\x8C\x8D";
    const auto bytes = encode(input);
    LV_CHECK_EQ(decode(bytes), input);
    LV_CHECK_EQ(modified_utf8_unit_count(input), 4u);
}

LV_TEST(modified_utf8_rejects_standard_four_byte_form) {
    const std::vector<std::uint8_t> bytes{0xF0, 0x9F, 0x98, 0x80};
    LV_CHECK_ERR(decode_modified_utf8(bytes), ErrorCode::malformed);
}

LV_TEST(modified_utf8_rejects_truncated_sequence) {
    const std::vector<std::uint8_t> two_byte{0xC3};
    LV_CHECK_ERR(decode_modified_utf8(two_byte), ErrorCode::malformed);

    const std::vector<std::uint8_t> three_byte{0xE4, 0xB8};
    LV_CHECK_ERR(decode_modified_utf8(three_byte), ErrorCode::malformed);
}

LV_TEST(modified_utf8_rejects_overlong_non_nul) {
    // C1 81 would decode to 'A'; only C0 80 (NUL) may be overlong.
    const std::vector<std::uint8_t> bytes{0xC1, 0x81};
    LV_CHECK_ERR(decode_modified_utf8(bytes), ErrorCode::malformed);
}

LV_TEST(modified_utf8_enforces_unit_limit) {
    const std::string input(10, 'x');
    std::vector<std::uint8_t> out;
    LV_CHECK_ERR(encode_modified_utf8(input, out, 4), ErrorCode::value_out_of_range);

    const std::vector<std::uint8_t> bytes(10, 'x');
    LV_CHECK_ERR(decode_modified_utf8(bytes, 4), ErrorCode::value_out_of_range);
}

LV_TEST(modified_utf8_counts_units_for_rejects) {
    // Invalid UTF-8 reports zero, which callers treat as "cannot encode".
    LV_CHECK_EQ(modified_utf8_unit_count("\xC3"), 0u);
    LV_CHECK_EQ(modified_utf8_unit_count("abc"), 3u);
}
