#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

// In-memory NBT tree.
//
// Design constraints that come from the parity requirements rather than taste:
//   - compound entries keep insertion order; reordering would break byte level
//     round trips even though the data is semantically identical
//   - lists keep their declared element type, including for empty lists, because
//     the type byte is part of the encoding
//   - byte arrays stay signed, matching the type byte the wire format uses

namespace lumenvox::nbt {

enum class Type : std::uint8_t {
    end = 0,
    byte_tag = 1,
    short_tag = 2,
    int_tag = 3,
    long_tag = 4,
    float_tag = 5,
    double_tag = 6,
    byte_array = 7,
    string = 8,
    list = 9,
    compound = 10,
    int_array = 11,
    long_array = 12,
};

[[nodiscard]] std::string_view to_string(Type type) noexcept;

class Tag;

using ByteArray = std::vector<std::int8_t>;
using IntArray = std::vector<std::int32_t>;
using LongArray = std::vector<std::int64_t>;

struct TagList {
    Type element_type = Type::end;
    std::vector<Tag> items;

    bool operator==(const TagList&) const = default;
};

struct TagCompound {
    std::vector<std::pair<std::string, Tag>> entries;

    [[nodiscard]] const Tag* find(std::string_view key) const;
    [[nodiscard]] Tag* find(std::string_view key);

    // Appends; duplicates are preserved because the format allows them and the
    // original does not reject them.
    Tag& emplace(std::string key, Tag tag);

    bool operator==(const TagCompound&) const = default;
};

class Tag {
public:
    Tag() = default;

    static Tag make_byte(std::int8_t value);
    static Tag make_short(std::int16_t value);
    static Tag make_int(std::int32_t value);
    static Tag make_long(std::int64_t value);
    static Tag make_float(float value);
    static Tag make_double(double value);
    static Tag make_byte_array(ByteArray value);
    static Tag make_string(std::string value);
    static Tag make_list(TagList value);
    static Tag make_compound(TagCompound value);
    static Tag make_int_array(IntArray value);
    static Tag make_long_array(LongArray value);

    [[nodiscard]] Type type() const noexcept;
    [[nodiscard]] bool is(Type type) const noexcept { return this->type() == type; }

    template <typename T>
    [[nodiscard]] const T* get_if() const {
        return std::get_if<T>(&storage_);
    }

    template <typename T>
    [[nodiscard]] T* get_if() {
        return std::get_if<T>(&storage_);
    }

    // Convenience for numeric scalars: widens any integer tag to 64 bits and
    // returns false for non numeric tags.
    [[nodiscard]] bool try_get_integer(std::int64_t& out) const noexcept;
    [[nodiscard]] bool try_get_f64(double& out) const noexcept;

    bool operator==(const Tag&) const = default;

private:
    using Storage = std::variant<std::monostate, std::int8_t, std::int16_t, std::int32_t,
                                 std::int64_t, float, double, ByteArray, std::string, TagList,
                                 TagCompound, IntArray, LongArray>;

    Storage storage_{};
};

}  // namespace lumenvox::nbt
