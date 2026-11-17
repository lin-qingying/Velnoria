#include <cstdint>
#include <vector>

#include "velnoria/core/byte_reader.hpp"
#include "velnoria/core/byte_writer.hpp"
#include "velnoria/core/varint.hpp"
#include "test_harness.hpp"

using velnoria::ByteReader;
using velnoria::ByteWriter;
using velnoria::ErrorCode;
using velnoria::read_varint;
using velnoria::read_varlong;
using velnoria::write_varint;
using velnoria::write_varlong;

namespace {

std::vector<std::uint8_t> encode(std::uint32_t value) {
    std::vector<std::uint8_t> buffer;
    ByteWriter writer(buffer);
    write_varint(value, writer);
    return buffer;
}

}  // namespace

LV_TEST(varint_known_encodings) {
    // Reference encodings taken from the published protocol specification.
    const struct {
        std::uint32_t value;
        std::vector<std::uint8_t> bytes;
    } cases[] = {
        {0u, {0x00}},
        {1u, {0x01}},
        {127u, {0x7F}},
        {128u, {0x80, 0x01}},
        {255u, {0xFF, 0x01}},
        {300u, {0xAC, 0x02}},
        {2147483647u, {0xFF, 0xFF, 0xFF, 0xFF, 0x07}},
    };

    for (const auto& test_case : cases) {
        const auto bytes = encode(test_case.value);
        LV_CHECK_EQ(bytes, test_case.bytes);

        ByteReader reader(bytes);
        auto decoded = read_varint(reader);
        LV_REQUIRE(decoded.ok());
        LV_CHECK_EQ(decoded.value(), test_case.value);
        LV_REQUIRE(reader.empty());
    }
}

LV_TEST(varint_roundtrip_boundaries) {
    for (std::uint32_t value = 0; value < 20000u; ++value) {
        const auto bytes = encode(value);
        LV_CHECK_EQ(bytes.size(), velnoria::varint_size(value));
        ByteReader reader(bytes);
        auto decoded = read_varint(reader);
        LV_REQUIRE(decoded.ok());
        if (decoded.value() != value) {
            ::lvtest::fail("roundtrip mismatch for " + std::to_string(value), __FILE__, __LINE__);
        }
    }
}

LV_TEST(varint_rejects_truncated) {
    const std::vector<std::uint8_t> bytes{0x80, 0x80};
    ByteReader reader(bytes);
    LV_CHECK_ERR(read_varint(reader), ErrorCode::truncated);
}

LV_TEST(varint_rejects_overlong) {
    // Six continuation bytes: never valid for a 32 bit varint.
    const std::vector<std::uint8_t> bytes{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01};
    ByteReader reader(bytes);
    LV_CHECK_ERR(read_varint(reader), ErrorCode::varint_too_long);
}

LV_TEST(varint_rejects_overflow_payload) {
    // Fifth byte carries bits beyond the 32 bit window.
    const std::vector<std::uint8_t> bytes{0xFF, 0xFF, 0xFF, 0xFF, 0x7F};
    ByteReader reader(bytes);
    LV_CHECK_ERR(read_varint(reader), ErrorCode::varint_overflow);
}

LV_TEST(varlong_roundtrip) {
    const std::vector<std::uint64_t> values{0ULL, 1ULL, 127ULL, 128ULL,
                                            2147483648ULL, 0xFFFFFFFFULL,
                                            0x1FFFFFFFFFFFFULL, 0x7FFFFFFFFFFFFFFFULL};
    for (auto value : values) {
        std::vector<std::uint8_t> buffer;
        ByteWriter writer(buffer);
        write_varlong(value, writer);

        ByteReader reader(buffer);
        auto decoded = read_varlong(reader);
        LV_REQUIRE(decoded.ok());
        LV_CHECK_EQ(decoded.value(), value);
    }
}
