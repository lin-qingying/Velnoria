#include "velnoria/core/modified_utf8.hpp"

namespace velnoria::text {
namespace {

void append_utf8(std::string& out, std::uint32_t code_point) {
    if (code_point < 0x80u) {
        out.push_back(static_cast<char>(code_point));
    } else if (code_point < 0x800u) {
        out.push_back(static_cast<char>(0xC0u | (code_point >> 6)));
        out.push_back(static_cast<char>(0x80u | (code_point & 0x3Fu)));
    } else if (code_point < 0x10000u) {
        out.push_back(static_cast<char>(0xE0u | (code_point >> 12)));
        out.push_back(static_cast<char>(0x80u | ((code_point >> 6) & 0x3Fu)));
        out.push_back(static_cast<char>(0x80u | (code_point & 0x3Fu)));
    } else {
        out.push_back(static_cast<char>(0xF0u | (code_point >> 18)));
        out.push_back(static_cast<char>(0x80u | ((code_point >> 12) & 0x3Fu)));
        out.push_back(static_cast<char>(0x80u | ((code_point >> 6) & 0x3Fu)));
        out.push_back(static_cast<char>(0x80u | (code_point & 0x3Fu)));
    }
}

Status append_modified_unit(std::vector<std::uint8_t>& out, std::uint16_t unit) {
    if (unit != 0 && unit < 0x80u) {
        out.push_back(static_cast<std::uint8_t>(unit));
        return Status::success();
    }
    if (unit < 0x800u) {
        out.push_back(static_cast<std::uint8_t>(0xC0u | (unit >> 6)));
        out.push_back(static_cast<std::uint8_t>(0x80u | (unit & 0x3Fu)));
        return Status::success();
    }
    out.push_back(static_cast<std::uint8_t>(0xE0u | (unit >> 12)));
    out.push_back(static_cast<std::uint8_t>(0x80u | ((unit >> 6) & 0x3Fu)));
    out.push_back(static_cast<std::uint8_t>(0x80u | (unit & 0x3Fu)));
    return Status::success();
}

}  // namespace

Result<std::string> decode_modified_utf8(std::span<const std::uint8_t> bytes,
                                         std::size_t max_units) {
    std::string out;
    out.reserve(bytes.size());

    std::size_t index = 0;
    std::size_t units = 0;

    while (index < bytes.size()) {
        const std::uint8_t first = bytes[index];
        std::uint16_t unit = 0;

        if (first < 0x80u) {
            unit = first;
            index += 1;
        } else if ((first & 0xE0u) == 0xC0u) {
            if (index + 1 >= bytes.size() || (bytes[index + 1] & 0xC0u) != 0x80u) {
                return Error{ErrorCode::malformed, "truncated two byte modified UTF-8 sequence"};
            }
            unit = static_cast<std::uint16_t>(((first & 0x1Fu) << 6) | (bytes[index + 1] & 0x3Fu));
            // Only the NUL escape is allowed to be overlong; anything else means
            // the encoder used standard UTF-8 and the bytes must not be accepted.
            if (unit < 0x80u && unit != 0) {
                return Error{ErrorCode::malformed, "overlong two byte modified UTF-8 sequence"};
            }
            index += 2;
        } else if ((first & 0xF0u) == 0xE0u) {
            if (index + 2 >= bytes.size() || (bytes[index + 1] & 0xC0u) != 0x80u ||
                (bytes[index + 2] & 0xC0u) != 0x80u) {
                return Error{ErrorCode::malformed, "truncated three byte modified UTF-8 sequence"};
            }
            unit = static_cast<std::uint16_t>(((first & 0x0Fu) << 12) |
                                              ((bytes[index + 1] & 0x3Fu) << 6) |
                                              (bytes[index + 2] & 0x3Fu));
            if (unit < 0x800u) {
                return Error{ErrorCode::malformed, "overlong three byte modified UTF-8 sequence"};
            }
            index += 3;
        } else {
            return Error{ErrorCode::malformed,
                         "four byte or continuation byte in modified UTF-8 stream"};
        }

        ++units;
        if (units > max_units) {
            return Error{ErrorCode::value_out_of_range, "modified UTF-8 string exceeds unit limit"};
        }

        if (unit >= 0xD800u && unit <= 0xDBFFu) {
            // High surrogate: a low surrogate must follow immediately for the
            // pair to be expressible as a single UTF-8 character. A lone
            // surrogate is not valid UTF-8, so it degrades to U+FFFD.
            if (index + 3 <= bytes.size() && (bytes[index] & 0xF0u) == 0xE0u &&
                (bytes[index + 1] & 0xC0u) == 0x80u && (bytes[index + 2] & 0xC0u) == 0x80u) {
                const std::uint16_t low = static_cast<std::uint16_t>(
                    ((bytes[index] & 0x0Fu) << 12) | ((bytes[index + 1] & 0x3Fu) << 6) |
                    (bytes[index + 2] & 0x3Fu));
                if (low >= 0xDC00u && low <= 0xDFFFu) {
                    const std::uint32_t code_point =
                        0x10000u + ((static_cast<std::uint32_t>(unit) - 0xD800u) << 10) +
                        (static_cast<std::uint32_t>(low) - 0xDC00u);
                    append_utf8(out, code_point);
                    index += 3;
                    ++units;
                    continue;
                }
            }
            append_utf8(out, 0xFFFDu);
            continue;
        }

        if (unit >= 0xDC00u && unit <= 0xDFFFu) {
            append_utf8(out, 0xFFFDu);
            continue;
        }

        append_utf8(out, unit);
    }

    return out;
}

std::size_t modified_utf8_unit_count(std::string_view utf8) {
    std::size_t units = 0;
    std::size_t index = 0;
    while (index < utf8.size()) {
        const auto first = static_cast<std::uint8_t>(utf8[index]);
        std::size_t width = 0;
        std::uint32_t code_point = 0;

        if (first < 0x80u) {
            width = 1;
            code_point = first;
        } else if ((first & 0xE0u) == 0xC0u) {
            width = 2;
            code_point = first & 0x1Fu;
        } else if ((first & 0xF0u) == 0xE0u) {
            width = 3;
            code_point = first & 0x0Fu;
        } else if ((first & 0xF8u) == 0xF0u) {
            width = 4;
            code_point = first & 0x07u;
        } else {
            return 0;
        }

        if (index + width > utf8.size()) {
            return 0;
        }
        for (std::size_t i = 1; i < width; ++i) {
            const auto continuation = static_cast<std::uint8_t>(utf8[index + i]);
            if ((continuation & 0xC0u) != 0x80u) {
                return 0;
            }
            code_point = (code_point << 6) | (continuation & 0x3Fu);
        }

        units += code_point >= 0x10000u ? 2u : 1u;
        index += width;
    }
    return units;
}

Status encode_modified_utf8(std::string_view utf8, std::vector<std::uint8_t>& out,
                            std::size_t max_units) {
    std::size_t units = 0;
    std::size_t index = 0;

    while (index < utf8.size()) {
        const auto first = static_cast<std::uint8_t>(utf8[index]);
        std::size_t width = 0;
        std::uint32_t code_point = 0;

        if (first < 0x80u) {
            width = 1;
            code_point = first;
        } else if ((first & 0xE0u) == 0xC0u) {
            width = 2;
            code_point = first & 0x1Fu;
        } else if ((first & 0xF0u) == 0xE0u) {
            width = 3;
            code_point = first & 0x0Fu;
        } else if ((first & 0xF8u) == 0xF0u) {
            width = 4;
            code_point = first & 0x07u;
        } else {
            return Status::fail(ErrorCode::malformed, "invalid UTF-8 start byte");
        }

        if (index + width > utf8.size()) {
            return Status::fail(ErrorCode::malformed, "truncated UTF-8 sequence");
        }
        for (std::size_t i = 1; i < width; ++i) {
            const auto continuation = static_cast<std::uint8_t>(utf8[index + i]);
            if ((continuation & 0xC0u) != 0x80u) {
                return Status::fail(ErrorCode::malformed, "invalid UTF-8 continuation byte");
            }
            code_point = (code_point << 6) | (continuation & 0x3Fu);
        }

        if (code_point >= 0x10000u) {
            units += 2;
            if (units > max_units) {
                return Status::fail(ErrorCode::value_out_of_range,
                                    "string exceeds modified UTF-8 unit limit");
            }
            const std::uint32_t adjusted = code_point - 0x10000u;
            const auto high = static_cast<std::uint16_t>(0xD800u + (adjusted >> 10));
            const auto low = static_cast<std::uint16_t>(0xDC00u + (adjusted & 0x3FFu));
            auto status = append_modified_unit(out, high);
            if (!status) {
                return status;
            }
            status = append_modified_unit(out, low);
            if (!status) {
                return status;
            }
        } else {
            ++units;
            if (units > max_units) {
                return Status::fail(ErrorCode::value_out_of_range,
                                    "string exceeds modified UTF-8 unit limit");
            }
            auto status = append_modified_unit(out, static_cast<std::uint16_t>(code_point));
            if (!status) {
                return status;
            }
        }

        index += width;
    }

    return Status::success();
}

}  // namespace velnoria::text
