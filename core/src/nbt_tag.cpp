#include "velnoria/nbt/tag.hpp"

namespace velnoria::nbt {

std::string_view to_string(Type type) noexcept {
    switch (type) {
        case Type::end:
            return "end";
        case Type::byte_tag:
            return "byte";
        case Type::short_tag:
            return "short";
        case Type::int_tag:
            return "int";
        case Type::long_tag:
            return "long";
        case Type::float_tag:
            return "float";
        case Type::double_tag:
            return "double";
        case Type::byte_array:
            return "byte_array";
        case Type::string:
            return "string";
        case Type::list:
            return "list";
        case Type::compound:
            return "compound";
        case Type::int_array:
            return "int_array";
        case Type::long_array:
            return "long_array";
    }
    return "unknown";
}

const Tag* TagCompound::find(std::string_view key) const {
    for (const auto& [name, tag] : entries) {
        if (name == key) {
            return &tag;
        }
    }
    return nullptr;
}

Tag* TagCompound::find(std::string_view key) {
    for (auto& [name, tag] : entries) {
        if (name == key) {
            return &tag;
        }
    }
    return nullptr;
}

Tag& TagCompound::emplace(std::string key, Tag tag) {
    entries.emplace_back(std::move(key), std::move(tag));
    return entries.back().second;
}

Tag Tag::make_byte(std::int8_t value) {
    Tag tag;
    tag.storage_ = value;
    return tag;
}

Tag Tag::make_short(std::int16_t value) {
    Tag tag;
    tag.storage_ = value;
    return tag;
}

Tag Tag::make_int(std::int32_t value) {
    Tag tag;
    tag.storage_ = value;
    return tag;
}

Tag Tag::make_long(std::int64_t value) {
    Tag tag;
    tag.storage_ = value;
    return tag;
}

Tag Tag::make_float(float value) {
    Tag tag;
    tag.storage_ = value;
    return tag;
}

Tag Tag::make_double(double value) {
    Tag tag;
    tag.storage_ = value;
    return tag;
}

Tag Tag::make_byte_array(ByteArray value) {
    Tag tag;
    tag.storage_ = std::move(value);
    return tag;
}

Tag Tag::make_string(std::string value) {
    Tag tag;
    tag.storage_ = std::move(value);
    return tag;
}

Tag Tag::make_list(TagList value) {
    Tag tag;
    tag.storage_ = std::move(value);
    return tag;
}

Tag Tag::make_compound(TagCompound value) {
    Tag tag;
    tag.storage_ = std::move(value);
    return tag;
}

Tag Tag::make_int_array(IntArray value) {
    Tag tag;
    tag.storage_ = std::move(value);
    return tag;
}

Tag Tag::make_long_array(LongArray value) {
    Tag tag;
    tag.storage_ = std::move(value);
    return tag;
}

Type Tag::type() const noexcept {
    // Relies on the variant alternative order matching the Type numbering.
    return static_cast<Type>(storage_.index());
}

bool Tag::try_get_integer(std::int64_t& out) const noexcept {
    if (const auto* value = std::get_if<std::int8_t>(&storage_)) {
        out = *value;
        return true;
    }
    if (const auto* value = std::get_if<std::int16_t>(&storage_)) {
        out = *value;
        return true;
    }
    if (const auto* value = std::get_if<std::int32_t>(&storage_)) {
        out = *value;
        return true;
    }
    if (const auto* value = std::get_if<std::int64_t>(&storage_)) {
        out = *value;
        return true;
    }
    return false;
}

bool Tag::try_get_f64(double& out) const noexcept {
    if (const auto* value = std::get_if<float>(&storage_)) {
        out = *value;
        return true;
    }
    if (const auto* value = std::get_if<double>(&storage_)) {
        out = *value;
        return true;
    }
    return false;
}

}  // namespace velnoria::nbt
