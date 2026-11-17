#include <cstdint>
#include <string>
#include <vector>

#include "lumenvox/core/md5.hpp"
#include "lumenvox/core/uuid.hpp"
#include "test_harness.hpp"

using lumenvox::Uuid;
using lumenvox::to_hex;

LV_TEST(md5_rfc1321_vectors) {
    // Expected digests computed independently with a reference implementation.
    const struct {
        const char* input;
        const char* digest;
    } cases[] = {
        {"", "d41d8cd98f00b204e9800998ecf8427e"},
        {"abc", "900150983cd24fb0d6963f7d28e17f72"},
        {"message digest", "f96b697d7cb7938d525a2f31aaf161d0"},
        {"abcdefghijklmnopqrstuvwxyz", "c3fcd3d76192e4007dfb496cca67e13b"},
    };

    for (const auto& test_case : cases) {
        LV_CHECK_EQ(to_hex(lumenvox::md5(std::string_view(test_case.input))),
                    std::string(test_case.digest));
    }
}

LV_TEST(md5_handles_block_boundaries) {
    // 55, 56, 63 and 64 bytes hit every padding path of the final block.
    const std::vector<std::size_t> sizes{0, 1, 55, 56, 57, 63, 64, 65, 119, 120};
    std::string previous;
    for (auto size : sizes) {
        const std::string input(size, 'a');
        const auto digest = to_hex(lumenvox::md5(std::string_view(input)));
        LV_REQUIRE(digest.size() == 32);
        LV_REQUIRE(digest != previous);
        previous = digest;
    }
}

LV_TEST(uuid_parse_and_format_roundtrip) {
    const char* text = "b50ad385-829d-3141-a216-7e7d7539ba7f";
    auto parsed = Uuid::parse(text);
    LV_REQUIRE(parsed.ok());
    LV_CHECK_EQ(parsed.value().to_string(), std::string(text));
    LV_CHECK_EQ(parsed.value().version(), 3);

    // Hyphen free form is accepted too.
    auto compact = Uuid::parse("b50ad385829d3141a2167e7d7539ba7f");
    LV_REQUIRE(compact.ok());
    LV_REQUIRE(compact.value() == parsed.value());
}

LV_TEST(uuid_parse_rejects_malformed_input) {
    LV_CHECK_ERR(Uuid::parse("b50ad385-829d-3141-a216"), lumenvox::ErrorCode::malformed);
    LV_CHECK_ERR(Uuid::parse("b50ad385-829d-3141-a216-7e7d7539ba7f0"),
                 lumenvox::ErrorCode::malformed);
    LV_CHECK_ERR(Uuid::parse("zb0ad385-829d-3141-a216-7e7d7539ba7f"),
                 lumenvox::ErrorCode::malformed);
    LV_CHECK_ERR(Uuid::parse("b50ad385-829d-3141-a216-7e7d7539ba7"),
                 lumenvox::ErrorCode::malformed);
}

LV_TEST(offline_player_uuid_matches_reference) {
    // nameUUIDFromBytes("OfflinePlayer:" + name), values computed independently.
    const struct {
        const char* name;
        const char* uuid;
    } cases[] = {
        {"Notch", "b50ad385-829d-3141-a216-7e7d7539ba7f"},
        {"jeb_", "a762f560-4fce-3236-812a-b80efff0b62b"},
        {"Player", "a01e3843-e521-3998-958a-f459800e4d11"},
    };

    for (const auto& test_case : cases) {
        const auto uuid = Uuid::offline_player(test_case.name);
        LV_CHECK_EQ(uuid.to_string(), std::string(test_case.uuid));
        LV_CHECK_EQ(uuid.version(), 3);
    }
}

LV_TEST(offline_player_uuid_is_case_sensitive) {
    LV_REQUIRE(Uuid::offline_player("notch") != Uuid::offline_player("Notch"));
}
