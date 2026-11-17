#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "lumenvox/core/byte_reader.hpp"
#include "lumenvox/core/byte_writer.hpp"
#include "lumenvox/nbt/io.hpp"
#include "lumenvox/nbt/tag.hpp"
#include "test_harness.hpp"

using lumenvox::ByteReader;
using lumenvox::ByteWriter;
using lumenvox::ErrorCode;
using lumenvox::nbt::Limits;
using lumenvox::nbt::NamedTag;
using lumenvox::nbt::Tag;
using lumenvox::nbt::TagCompound;
using lumenvox::nbt::TagList;
using lumenvox::nbt::Type;

namespace {

using Bytes = std::vector<std::uint8_t>;

Bytes write_tag(const NamedTag& named) {
    Bytes bytes;
    ByteWriter writer(bytes);
    auto status = lumenvox::nbt::write_root(named, writer);
    LV_REQUIRE(status.ok());
    return bytes;
}

NamedTag read_tag(const Bytes& bytes, Limits limits = {}) {
    ByteReader reader(bytes);
    auto result = lumenvox::nbt::read_root(reader, limits);
    LV_REQUIRE(result.ok());
    return std::move(result).take();
}

Tag compound_of(TagCompound compound) {
    return Tag::make_compound(std::move(compound));
}

}  // namespace

LV_TEST(nbt_encodes_expected_bytes) {
    TagCompound root;
    root.emplace("b", Tag::make_byte(7));
    root.emplace("s", Tag::make_short(0x0102));
    root.emplace("i", Tag::make_int(0x01020304));
    root.emplace("l", Tag::make_long(0x0102030405060708LL));
    root.emplace("t", Tag::make_string("hi"));

    const NamedTag named{"", compound_of(std::move(root))};
    const Bytes expected{
        0x0A, 0x00, 0x00,                                            // compound, empty name
        0x01, 0x00, 0x01, 'b', 0x07,                                 // byte b = 7
        0x02, 0x00, 0x01, 's', 0x01, 0x02,                           // short s
        0x03, 0x00, 0x01, 'i', 0x01, 0x02, 0x03, 0x04,               // int i
        0x04, 0x00, 0x01, 'l', 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
        0x08, 0x00, 0x01, 't', 0x00, 0x02, 'h', 'i',                 // string t = "hi"
        0x00,                                                        // end of compound
    };

    const auto bytes = write_tag(named);
    LV_CHECK_EQ(bytes, expected);

    const auto decoded = read_tag(bytes);
    LV_REQUIRE(decoded.tag == named.tag);
    LV_CHECK_EQ(write_tag(decoded), expected);
}

LV_TEST(nbt_preserves_compound_entry_order) {
    TagCompound root;
    root.emplace("z", Tag::make_int(1));
    root.emplace("a", Tag::make_int(2));
    root.emplace("m", Tag::make_int(3));

    const auto bytes = write_tag(NamedTag{"", compound_of(std::move(root))});
    const auto decoded = read_tag(bytes);

    const auto* compound = decoded.tag.get_if<TagCompound>();
    LV_REQUIRE(compound != nullptr);
    LV_REQUIRE(compound->entries.size() == 3);
    LV_CHECK_EQ(compound->entries[0].first, std::string("z"));
    LV_CHECK_EQ(compound->entries[1].first, std::string("a"));
    LV_CHECK_EQ(compound->entries[2].first, std::string("m"));
}

LV_TEST(nbt_keeps_element_type_of_empty_list) {
    TagList list;
    list.element_type = Type::int_tag;  // typed but empty
    TagCompound root;
    root.emplace("l", Tag::make_list(std::move(list)));

    const Bytes expected{
        0x0A, 0x00, 0x00,
        0x09, 0x00, 0x01, 'l', 0x03, 0x00, 0x00, 0x00, 0x00,
        0x00,
    };
    const auto bytes = write_tag(NamedTag{"", compound_of(std::move(root))});
    LV_CHECK_EQ(bytes, expected);

    const auto decoded = read_tag(bytes);
    const auto* compound = decoded.tag.get_if<TagCompound>();
    LV_REQUIRE(compound != nullptr);
    const Tag* field = compound->find("l");
    LV_REQUIRE(field != nullptr);
    const auto* decoded_list = field->get_if<TagList>();
    LV_REQUIRE(decoded_list != nullptr);
    LV_REQUIRE(decoded_list->element_type == Type::int_tag);
    LV_REQUIRE(decoded_list->items.empty());
}

LV_TEST(nbt_roundtrips_all_array_and_numeric_types) {
    TagCompound root;
    root.emplace("ba", Tag::make_byte_array({1, -2, 3}));
    root.emplace("ia", Tag::make_int_array({1, -2, 70000}));
    root.emplace("la", Tag::make_long_array({1, -2, 5000000000LL}));
    root.emplace("f", Tag::make_float(1.5F));
    root.emplace("d", Tag::make_double(-2.25));

    TagList list;
    list.element_type = Type::string;
    list.items.push_back(Tag::make_string("x"));
    list.items.push_back(Tag::make_string("y"));
    root.emplace("ls", Tag::make_list(std::move(list)));

    TagCompound inner;
    inner.emplace("deep", Tag::make_byte(-1));
    root.emplace("ic", compound_of(std::move(inner)));

    const NamedTag named{"root", compound_of(std::move(root))};
    const auto bytes = write_tag(named);
    const auto decoded = read_tag(bytes);

    LV_REQUIRE(decoded.name == std::string("root"));
    LV_REQUIRE(decoded.tag == named.tag);
    LV_CHECK_EQ(write_tag(decoded), bytes);
}

LV_TEST(nbt_roundtrips_nul_inside_string) {
    TagCompound root;
    root.emplace("t", Tag::make_string(std::string("a\0b", 3)));

    const auto bytes = write_tag(NamedTag{"", compound_of(std::move(root))});
    // Entry followed by the compound terminator: three units, four bytes, NUL as C0 80.
    const Bytes tail{0x08, 0x00, 0x01, 't', 0x00, 0x03, 0x61, 0xC0, 0x80, 0x62, 0x00};
    LV_REQUIRE(bytes.size() > tail.size());
    LV_REQUIRE(std::equal(tail.begin(), tail.end(), bytes.end() - static_cast<long>(tail.size())));

    const auto decoded = read_tag(bytes);
    const auto* compound = decoded.tag.get_if<TagCompound>();
    LV_REQUIRE(compound != nullptr);
    const Tag* field = compound->find("t");
    LV_REQUIRE(field != nullptr);
    LV_REQUIRE(*field->get_if<std::string>() == std::string("a\0b", 3));
}

LV_TEST(nbt_rejects_truncated_input) {
    TagCompound root;
    root.emplace("i", Tag::make_int(5));
    auto bytes = write_tag(NamedTag{"", compound_of(std::move(root))});
    bytes.pop_back();
    bytes.pop_back();

    ByteReader reader(bytes);
    LV_CHECK_ERR(lumenvox::nbt::read_root(reader), ErrorCode::truncated);
}

LV_TEST(nbt_rejects_negative_list_length) {
    const Bytes bytes{
        0x0A, 0x00, 0x00,
        0x09, 0x00, 0x01, 'l', 0x01, 0xFF, 0xFF, 0xFF, 0xFF,
        0x00,
    };
    ByteReader reader(bytes);
    LV_CHECK_ERR(lumenvox::nbt::read_root(reader), ErrorCode::malformed);
}

LV_TEST(nbt_rejects_unknown_tag_type) {
    const Bytes bytes{0x0A, 0x00, 0x00, 0x3F, 0x00, 0x01, 'x', 0x00, 0x00};
    ByteReader reader(bytes);
    LV_CHECK_ERR(lumenvox::nbt::read_root(reader), ErrorCode::malformed);
}

LV_TEST(nbt_enforces_depth_limit) {
    // Ten nested compounds; the limit is three.
    Tag current = compound_of(TagCompound{});
    for (int i = 0; i < 10; ++i) {
        TagCompound wrapper;
        wrapper.emplace("c", current);
        current = compound_of(std::move(wrapper));
    }

    const auto bytes = write_tag(NamedTag{"", current});
    Limits limits;
    limits.max_depth = 3;
    ByteReader reader(bytes);
    LV_CHECK_ERR(lumenvox::nbt::read_root(reader, limits), ErrorCode::value_out_of_range);
}

LV_TEST(nbt_enforces_string_unit_limit) {
    TagCompound root;
    root.emplace("t", Tag::make_string("abcdef"));
    const auto bytes = write_tag(NamedTag{"", compound_of(std::move(root))});

    Limits limits;
    limits.max_string_units = 3;
    ByteReader reader(bytes);
    LV_CHECK_ERR(lumenvox::nbt::read_root(reader, limits), ErrorCode::value_out_of_range);
}

LV_TEST(nbt_enforces_byte_budget) {
    TagCompound root;
    root.emplace("lb", Tag::make_long_array({1, 2, 3, 4, 5, 6, 7, 8}));
    const auto bytes = write_tag(NamedTag{"", compound_of(std::move(root))});

    Limits limits;
    limits.max_bytes = 8;
    ByteReader reader(bytes);
    LV_CHECK_ERR(lumenvox::nbt::read_root(reader, limits), ErrorCode::frame_too_large);
}
