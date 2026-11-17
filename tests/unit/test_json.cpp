#include <cstdint>
#include <string>
#include <cstdio>

#include "velnoria/core/json.hpp"
#include "test_harness.hpp"

using velnoria::ErrorCode;
using velnoria::json::parse;
using velnoria::json::Value;

LV_TEST(json_parses_objects_arrays_and_scalars) {
    const std::string text =
        R"({"name": "alpha", "count": 42, "ratio": -1.5e2, "flag": true, "none": null,
            "list": [1, 2, {"inner": "x"}]})";

    auto result = parse(text);
    LV_REQUIRE(result.ok());
    const Value& root = result.value();

    LV_REQUIRE(root.is_object());
    LV_REQUIRE(root.find("name") != nullptr);
    LV_CHECK_EQ(root.find("name")->as_string(), std::string("alpha"));
    LV_CHECK_EQ(root.find("count")->as_int64(), 42);
    LV_CHECK_EQ(root.find("ratio")->as_number(), -150.0);
    LV_REQUIRE(root.find("flag")->as_bool());
    LV_REQUIRE(root.find("none")->is_null());
    LV_REQUIRE(root.find("list")->items().size() == 3);
    LV_CHECK_EQ(root.find("list")->items()[2].find("inner")->as_string(), std::string("x"));
}

LV_TEST(json_preserves_member_order) {
    auto result = parse(R"({"z": 1, "a": 2, "m": 3})");
    LV_REQUIRE(result.ok());
    const auto& members = result.value().members();
    LV_REQUIRE(members.size() == 3);
    LV_CHECK_EQ(members[0].first, std::string("z"));
    LV_CHECK_EQ(members[1].first, std::string("a"));
    LV_CHECK_EQ(members[2].first, std::string("m"));
}

LV_TEST(json_decodes_escapes_and_surrogates) {
    auto result = parse(R"({"s": "a\nb\t\"q\"\\", "u": "\u00e9", "astral": "\ud83d\ude00"})");
    LV_REQUIRE(result.ok());
    LV_CHECK_EQ(result.value().find("s")->as_string(), std::string("a\nb\t\"q\"\\"));
    LV_CHECK_EQ(result.value().find("u")->as_string(), std::string("\xC3\xA9"));
    LV_CHECK_EQ(result.value().find("astral")->as_string(), std::string("\xF0\x9F\x98\x80"));
}

LV_TEST(json_rejects_malformed_input) {
    // Written with explicit strings rather than raw literals: adjacent raw
    // literals with braces are easy to misread, and a misread test is worse
    // than no test.
    struct Case {
        const char* text;
        velnoria::ErrorCode expected;
    };
    const Case cases[] = {
        {"{\"a\": 1}x", velnoria::ErrorCode::malformed},
        {"{\"a\": 1", velnoria::ErrorCode::truncated},
        {"{\"a\" 1}", velnoria::ErrorCode::malformed},
        {"[1,]", velnoria::ErrorCode::malformed},
        {"\"abc", velnoria::ErrorCode::truncated},
        {"{\"a\": \"\\ud83d\"}", velnoria::ErrorCode::malformed},
        {"tru", velnoria::ErrorCode::malformed},
    };

    for (const auto& test_case : cases) {
        auto result = parse(test_case.text);
        LV_REQUIRE(!result.ok());
        if (result.error().code != test_case.expected) {
            ::lvtest::fail(std::string("input <") + test_case.text + "> expected " +
                               std::string(velnoria::to_string(test_case.expected)) + ", got " +
                               std::string(velnoria::to_string(result.error().code)),
                           __FILE__, __LINE__);
        }
    }
}

LV_TEST(json_enforces_depth_limit) {
    std::string text;
    for (int i = 0; i < 20; ++i) {
        text.push_back('[');
    }
    for (int i = 0; i < 20; ++i) {
        text.push_back(']');
    }

    velnoria::json::Limits limits;
    limits.max_depth = 4;
    LV_CHECK_ERR(parse(text, limits), ErrorCode::value_out_of_range);
}

LV_TEST(json_rejects_oversized_input) {
    velnoria::json::Limits limits;
    limits.max_bytes = 4;
    LV_CHECK_ERR(parse("{\"a\":1}", limits), ErrorCode::frame_too_large);
}

LV_TEST(json_integer_extraction_rejects_fractions) {
    auto result = parse(R"({"a": 1.5, "b": 7})");
    LV_REQUIRE(result.ok());
    LV_CHECK_EQ(result.value().find("a")->as_int64(), 0);
    LV_CHECK_EQ(result.value().find("b")->as_int64(), 7);
}

