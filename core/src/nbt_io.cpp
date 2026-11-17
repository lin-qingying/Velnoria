#include "lumenvox/nbt/io.hpp"

#include <cstring>
#include <utility>
#include <vector>

#include "lumenvox/core/modified_utf8.hpp"

namespace lumenvox::nbt {
namespace {

struct Context {
    ByteReader& reader;
    const Limits& limits;
    std::size_t start_position;
    int depth;
};

Status check_budget(const Context& ctx) {
    if (ctx.reader.position() - ctx.start_position > ctx.limits.max_bytes) {
        return Status::fail(ErrorCode::frame_too_large, "NBT payload exceeds byte budget");
    }
    if (ctx.depth > ctx.limits.max_depth) {
        return Status::fail(ErrorCode::value_out_of_range, "NBT nesting exceeds depth limit");
    }
    return Status::success();
}

Result<std::uint16_t> read_u16(Context& ctx) {
    auto bytes = ctx.reader.read_bytes(2);
    if (!bytes) {
        return bytes.error();
    }
    const auto* data = bytes.value().data();
    auto budget = check_budget(ctx);
    if (!budget) {
        return budget.error();
    }
    return static_cast<std::uint16_t>((data[0] << 8) | data[1]);
}

Result<std::uint32_t> read_u32(Context& ctx) {
    auto bytes = ctx.reader.read_bytes(4);
    if (!bytes) {
        return bytes.error();
    }
    const auto* data = bytes.value().data();
    auto budget = check_budget(ctx);
    if (!budget) {
        return budget.error();
    }
    return (static_cast<std::uint32_t>(data[0]) << 24) |
           (static_cast<std::uint32_t>(data[1]) << 16) |
           (static_cast<std::uint32_t>(data[2]) << 8) | static_cast<std::uint32_t>(data[3]);
}

Result<std::uint64_t> read_u64(Context& ctx) {
    auto bytes = ctx.reader.read_bytes(8);
    if (!bytes) {
        return bytes.error();
    }
    const auto* data = bytes.value().data();
    std::uint64_t value = 0;
    for (int i = 0; i < 8; ++i) {
        value = (value << 8) | data[i];
    }
    auto budget = check_budget(ctx);
    if (!budget) {
        return budget.error();
    }
    return value;
}

// Reads exactly `units` UTF-16 code units worth of modified UTF-8, then decodes
// them. Reading and decoding are split this way so the caller controls how many
// bytes are consumed: the length prefix counts code units, not bytes.
Result<std::string> read_string_units(Context& ctx, std::size_t units) {
    if (units > ctx.limits.max_string_units) {
        return Error{ErrorCode::value_out_of_range, "NBT string exceeds unit limit"};
    }

    std::vector<std::uint8_t> raw;
    raw.reserve(units);

    std::size_t consumed = 0;
    while (consumed < units) {
        auto first = ctx.reader.read_u8();
        if (!first) {
            return first.error();
        }
        const std::uint8_t lead = first.value();
        raw.push_back(lead);

        std::size_t extra = 0;
        if (lead < 0x80u) {
            extra = 0;
        } else if ((lead & 0xE0u) == 0xC0u) {
            extra = 1;
        } else if ((lead & 0xF0u) == 0xE0u) {
            extra = 2;
        } else {
            return Error{ErrorCode::malformed, "invalid lead byte in NBT string"};
        }

        for (std::size_t i = 0; i < extra; ++i) {
            auto next = ctx.reader.read_u8();
            if (!next) {
                return next.error();
            }
            raw.push_back(next.value());
        }
        ++consumed;

        auto budget = check_budget(ctx);
        if (!budget) {
            return budget.error();
        }
    }

    return text::decode_modified_utf8(raw, ctx.limits.max_string_units);
}

Result<std::string> read_name(Context& ctx) {
    auto length = read_u16(ctx);
    if (!length) {
        return length.error();
    }
    return read_string_units(ctx, length.value());
}

Result<Tag> read_payload_impl(Context& ctx, Type type);

Result<Tag> read_list(Context& parent) {
    Context ctx{parent.reader, parent.limits, parent.start_position, parent.depth + 1};
    auto budget = check_budget(ctx);
    if (!budget) {
        return budget.error();
    }

    auto element_type_byte = ctx.reader.read_u8();
    if (!element_type_byte) {
        return element_type_byte.error();
    }
    const auto element_type = static_cast<Type>(element_type_byte.value());
    if (element_type > Type::long_array) {
        return Error{ErrorCode::malformed, "invalid NBT list element type"};
    }

    auto length = read_u32(ctx);
    if (!length) {
        return length.error();
    }
    const auto count = static_cast<std::size_t>(static_cast<std::int32_t>(length.value()));
    if (static_cast<std::int32_t>(length.value()) < 0) {
        return Error{ErrorCode::malformed, "negative NBT list length"};
    }
    if (count > ctx.limits.max_collection) {
        return Error{ErrorCode::value_out_of_range, "NBT list exceeds element limit"};
    }

    TagList list;
    list.element_type = element_type;
    if (count == 0) {
        return Tag::make_list(std::move(list));
    }
    if (element_type == Type::end) {
        return Error{ErrorCode::malformed, "non empty NBT list of end type"};
    }

    list.items.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        auto item = read_payload_impl(ctx, element_type);
        if (!item) {
            return item.error();
        }
        list.items.push_back(std::move(item).take());
    }
    return Tag::make_list(std::move(list));
}

Result<Tag> read_compound(Context& ctx) {
    Context nested{ctx.reader, ctx.limits, ctx.start_position, ctx.depth + 1};
    auto budget = check_budget(nested);
    if (!budget) {
        return budget.error();
    }

    TagCompound compound;
    std::size_t entries = 0;

    while (true) {
        auto type_byte = nested.reader.read_u8();
        if (!type_byte) {
            return type_byte.error();
        }
        if (type_byte.value() == 0) {
            return Tag::make_compound(std::move(compound));
        }
        const auto type = static_cast<Type>(type_byte.value());
        if (type > Type::long_array) {
            return Error{ErrorCode::malformed, "invalid NBT tag type in compound"};
        }

        auto name = read_name(nested);
        if (!name) {
            return name.error();
        }
        if (++entries > nested.limits.max_collection) {
            return Error{ErrorCode::value_out_of_range, "NBT compound exceeds entry limit"};
        }

        auto value = read_payload_impl(nested, type);
        if (!value) {
            return value.error();
        }
        compound.entries.emplace_back(std::move(name).take(), std::move(value).take());
    }
}

Result<Tag> read_payload_impl(Context& ctx, Type type) {
    auto budget = check_budget(ctx);
    if (!budget) {
        return budget.error();
    }

    switch (type) {
        case Type::end:
            return Tag{};
        case Type::byte_tag: {
            auto value = ctx.reader.read_u8();
            if (!value) {
                return value.error();
            }
            return Tag::make_byte(static_cast<std::int8_t>(value.value()));
        }
        case Type::short_tag: {
            auto value = read_u16(ctx);
            if (!value) {
                return value.error();
            }
            return Tag::make_short(static_cast<std::int16_t>(value.value()));
        }
        case Type::int_tag: {
            auto value = read_u32(ctx);
            if (!value) {
                return value.error();
            }
            return Tag::make_int(static_cast<std::int32_t>(value.value()));
        }
        case Type::long_tag: {
            auto value = read_u64(ctx);
            if (!value) {
                return value.error();
            }
            return Tag::make_long(static_cast<std::int64_t>(value.value()));
        }
        case Type::float_tag: {
            auto value = read_u32(ctx);
            if (!value) {
                return value.error();
            }
            const std::uint32_t bits = value.value();
            float result = 0.0F;
            std::memcpy(&result, &bits, sizeof(result));
            return Tag::make_float(result);
        }
        case Type::double_tag: {
            auto value = read_u64(ctx);
            if (!value) {
                return value.error();
            }
            const std::uint64_t bits = value.value();
            double result = 0.0;
            std::memcpy(&result, &bits, sizeof(result));
            return Tag::make_double(result);
        }
        case Type::byte_array: {
            auto length = read_u32(ctx);
            if (!length) {
                return length.error();
            }
            const auto count = static_cast<std::int32_t>(length.value());
            if (count < 0) {
                return Error{ErrorCode::malformed, "negative NBT byte array length"};
            }
            if (static_cast<std::size_t>(count) > ctx.limits.max_collection) {
                return Error{ErrorCode::value_out_of_range, "NBT byte array exceeds element limit"};
            }
            if (static_cast<std::size_t>(count) > ctx.limits.max_bytes) {
                return Error{ErrorCode::frame_too_large, "NBT byte array exceeds byte budget"};
            }
            auto bytes = ctx.reader.read_bytes(static_cast<std::size_t>(count));
            if (!bytes) {
                return bytes.error();
            }
            ByteArray array(static_cast<std::size_t>(count));
            for (std::size_t i = 0; i < array.size(); ++i) {
                array[i] = static_cast<std::int8_t>(bytes.value()[i]);
            }
            return Tag::make_byte_array(std::move(array));
        }
        case Type::string: {
            auto length = read_u16(ctx);
            if (!length) {
                return length.error();
            }
            auto text = read_string_units(ctx, length.value());
            if (!text) {
                return text.error();
            }
            return Tag::make_string(std::move(text).take());
        }
        case Type::list:
            return read_list(ctx);
        case Type::compound:
            return read_compound(ctx);
        case Type::int_array: {
            auto length = read_u32(ctx);
            if (!length) {
                return length.error();
            }
            const auto signed_length = static_cast<std::int32_t>(length.value());
            if (signed_length < 0) {
                return Error{ErrorCode::malformed, "negative NBT int array length"};
            }
            const auto count = static_cast<std::size_t>(signed_length);
            if (count > ctx.limits.max_collection) {
                return Error{ErrorCode::value_out_of_range, "NBT int array exceeds element limit"};
            }
            IntArray array(count);
            for (std::size_t i = 0; i < count; ++i) {
                auto value = read_u32(ctx);
                if (!value) {
                    return value.error();
                }
                array[i] = static_cast<std::int32_t>(value.value());
            }
            return Tag::make_int_array(std::move(array));
        }
        case Type::long_array: {
            auto length = read_u32(ctx);
            if (!length) {
                return length.error();
            }
            const auto signed_length = static_cast<std::int32_t>(length.value());
            if (signed_length < 0) {
                return Error{ErrorCode::malformed, "negative NBT long array length"};
            }
            const auto count = static_cast<std::size_t>(signed_length);
            if (count > ctx.limits.max_collection) {
                return Error{ErrorCode::value_out_of_range, "NBT long array exceeds element limit"};
            }
            LongArray array(count);
            for (std::size_t i = 0; i < count; ++i) {
                auto value = read_u64(ctx);
                if (!value) {
                    return value.error();
                }
                array[i] = static_cast<std::int64_t>(value.value());
            }
            return Tag::make_long_array(std::move(array));
        }
    }

    return Error{ErrorCode::malformed, "unhandled NBT tag type"};
}

void write_u16(std::uint16_t value, ByteWriter& writer) {
    writer.write_u8(static_cast<std::uint8_t>(value >> 8));
    writer.write_u8(static_cast<std::uint8_t>(value & 0xFFu));
}

void write_u32(std::uint32_t value, ByteWriter& writer) {
    writer.write_u8(static_cast<std::uint8_t>((value >> 24) & 0xFFu));
    writer.write_u8(static_cast<std::uint8_t>((value >> 16) & 0xFFu));
    writer.write_u8(static_cast<std::uint8_t>((value >> 8) & 0xFFu));
    writer.write_u8(static_cast<std::uint8_t>(value & 0xFFu));
}

void write_u64(std::uint64_t value, ByteWriter& writer) {
    for (int shift = 56; shift >= 0; shift -= 8) {
        writer.write_u8(static_cast<std::uint8_t>((value >> shift) & 0xFFu));
    }
}

Status write_string(std::string_view value, ByteWriter& writer) {
    std::vector<std::uint8_t> encoded;
    auto status = text::encode_modified_utf8(value, encoded);
    if (!status) {
        return status;
    }
    const auto units = text::modified_utf8_unit_count(value);
    if (units > 0xFFFFu) {
        return Status::fail(ErrorCode::value_out_of_range, "NBT string too long for u16 prefix");
    }
    write_u16(static_cast<std::uint16_t>(units), writer);
    writer.write_bytes(encoded);
    return Status::success();
}

Status write_payload_impl(const Tag& tag, ByteWriter& writer) {
    switch (tag.type()) {
        case Type::end:
            return Status::success();
        case Type::byte_tag:
            writer.write_u8(static_cast<std::uint8_t>(*tag.get_if<std::int8_t>()));
            return Status::success();
        case Type::short_tag:
            write_u16(static_cast<std::uint16_t>(*tag.get_if<std::int16_t>()), writer);
            return Status::success();
        case Type::int_tag:
            write_u32(static_cast<std::uint32_t>(*tag.get_if<std::int32_t>()), writer);
            return Status::success();
        case Type::long_tag:
            write_u64(static_cast<std::uint64_t>(*tag.get_if<std::int64_t>()), writer);
            return Status::success();
        case Type::float_tag: {
            const float value = *tag.get_if<float>();
            std::uint32_t bits = 0;
            std::memcpy(&bits, &value, sizeof(bits));
            write_u32(bits, writer);
            return Status::success();
        }
        case Type::double_tag: {
            const double value = *tag.get_if<double>();
            std::uint64_t bits = 0;
            std::memcpy(&bits, &value, sizeof(bits));
            write_u64(bits, writer);
            return Status::success();
        }
        case Type::byte_array: {
            const auto& array = *tag.get_if<ByteArray>();
            write_u32(static_cast<std::uint32_t>(array.size()), writer);
            for (auto value : array) {
                writer.write_u8(static_cast<std::uint8_t>(value));
            }
            return Status::success();
        }
        case Type::string:
            return write_string(*tag.get_if<std::string>(), writer);
        case Type::list: {
            const auto& list = *tag.get_if<TagList>();
            if (list.items.empty() && list.element_type != Type::end) {
                // A typed but empty list keeps its declared element type.
                writer.write_u8(static_cast<std::uint8_t>(list.element_type));
                write_u32(0u, writer);
                return Status::success();
            }
            if (list.element_type == Type::end && !list.items.empty()) {
                return Status::fail(ErrorCode::malformed, "non empty list declared as end type");
            }
            writer.write_u8(static_cast<std::uint8_t>(list.element_type));
            write_u32(static_cast<std::uint32_t>(list.items.size()), writer);
            for (const auto& item : list.items) {
                if (item.type() != list.element_type) {
                    return Status::fail(ErrorCode::malformed,
                                        "list item type does not match declared element type");
                }
                auto status = write_payload_impl(item, writer);
                if (!status) {
                    return status;
                }
            }
            return Status::success();
        }
        case Type::compound: {
            const auto& compound = *tag.get_if<TagCompound>();
            for (const auto& [name, value] : compound.entries) {
                if (value.type() == Type::end) {
                    return Status::fail(ErrorCode::malformed, "end tag cannot be a compound entry");
                }
                writer.write_u8(static_cast<std::uint8_t>(value.type()));
                auto status = write_string(name, writer);
                if (!status) {
                    return status;
                }
                status = write_payload_impl(value, writer);
                if (!status) {
                    return status;
                }
            }
            writer.write_u8(0u);
            return Status::success();
        }
        case Type::int_array: {
            const auto& array = *tag.get_if<IntArray>();
            write_u32(static_cast<std::uint32_t>(array.size()), writer);
            for (auto value : array) {
                write_u32(static_cast<std::uint32_t>(value), writer);
            }
            return Status::success();
        }
        case Type::long_array: {
            const auto& array = *tag.get_if<LongArray>();
            write_u32(static_cast<std::uint32_t>(array.size()), writer);
            for (auto value : array) {
                write_u64(static_cast<std::uint64_t>(value), writer);
            }
            return Status::success();
        }
    }
    return Status::fail(ErrorCode::malformed, "unhandled NBT tag type");
}

}  // namespace

Result<NamedTag> read_root(ByteReader& reader, Limits limits) {
    auto type_byte = reader.read_u8();
    if (!type_byte) {
        return type_byte.error();
    }
    if (type_byte.value() == 0) {
        return NamedTag{"", Tag{}};
    }
    const auto type = static_cast<Type>(type_byte.value());
    if (type > Type::long_array) {
        return Error{ErrorCode::malformed, "invalid NBT root tag type"};
    }

    Context ctx{reader, limits, reader.position(), 0};
    auto name = read_name(ctx);
    if (!name) {
        return name.error();
    }
    auto payload = read_payload_impl(ctx, type);
    if (!payload) {
        return payload.error();
    }
    return NamedTag{std::move(name).take(), std::move(payload).take()};
}

Result<Tag> read_payload(ByteReader& reader, Type type, const Limits& limits) {
    Context ctx{reader, limits, reader.position(), 0};
    return read_payload_impl(ctx, type);
}

Status write_root(const NamedTag& named, ByteWriter& writer) {
    if (named.tag.type() == Type::end) {
        writer.write_u8(0u);
        return Status::success();
    }
    writer.write_u8(static_cast<std::uint8_t>(named.tag.type()));
    auto status = write_string(named.name, writer);
    if (!status) {
        return status;
    }
    return write_payload_impl(named.tag, writer);
}

Status write_payload(const Tag& tag, ByteWriter& writer) {
    return write_payload_impl(tag, writer);
}

}  // namespace lumenvox::nbt
