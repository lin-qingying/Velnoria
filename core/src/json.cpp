#include "velnoria/core/json.hpp"

#include <cmath>
#include <cstdlib>

namespace velnoria::json {
namespace {

bool is_whitespace(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

bool is_digit(char c) {
    return c >= '0' && c <= '9';
}

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

int hex_digit(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

}  // namespace

class Parser {
public:
    Parser(std::string_view text, Limits limits) : text_(text), limits_(limits) {}

    Result<Value> run() {
        if (text_.size() > limits_.max_bytes) {
            return Error{ErrorCode::frame_too_large, "JSON input exceeds byte limit"};
        }
        auto value = parse_value(0);
        if (!value) {
            return value;
        }
        skip_whitespace();
        if (position_ != text_.size()) {
            return Error{ErrorCode::malformed, "trailing content after JSON value"};
        }
        return value;
    }

private:
    void skip_whitespace() {
        while (position_ < text_.size() && is_whitespace(text_[position_])) {
            ++position_;
        }
    }

    [[nodiscard]] bool at_end() const { return position_ >= text_.size(); }
    [[nodiscard]] char peek() const { return text_[position_]; }

    Result<Value> parse_value(int depth) {
        if (depth > limits_.max_depth) {
            return Error{ErrorCode::value_out_of_range, "JSON nesting exceeds depth limit"};
        }
        skip_whitespace();
        if (at_end()) {
            return Error{ErrorCode::truncated, "unexpected end of JSON"};
        }

        const char c = peek();
        if (c == '{') {
            return parse_object(depth);
        }
        if (c == '[') {
            return parse_array(depth);
        }
        if (c == '"') {
            auto text = parse_string();
            if (!text) {
                return text.error();
            }
            Value value;
            value.kind_ = Kind::string;
            value.string_ = std::move(text).take();
            return value;
        }
        if (c == 't' || c == 'f') {
            return parse_bool();
        }
        if (c == 'n') {
            if (text_.compare(position_, 4, "null") == 0) {
                position_ += 4;
                Value value;
                value.kind_ = Kind::null_value;
                return value;
            }
            return Error{ErrorCode::malformed, "invalid literal in JSON"};
        }
        if (c == '-' || is_digit(c)) {
            return parse_number();
        }
        return Error{ErrorCode::malformed, "unexpected character in JSON"};
    }

    Result<Value> parse_bool() {
        Value value;
        value.kind_ = Kind::boolean;
        if (text_.compare(position_, 4, "true") == 0) {
            position_ += 4;
            value.boolean_ = true;
            return value;
        }
        if (text_.compare(position_, 5, "false") == 0) {
            position_ += 5;
            value.boolean_ = false;
            return value;
        }
        return Error{ErrorCode::malformed, "invalid boolean literal in JSON"};
    }

    Result<Value> parse_number() {
        const std::size_t start = position_;
        if (peek() == '-') {
            ++position_;
        }
        while (!at_end() && is_digit(peek())) {
            ++position_;
        }
        if (!at_end() && peek() == '.') {
            ++position_;
            while (!at_end() && is_digit(peek())) {
                ++position_;
            }
        }
        if (!at_end() && (peek() == 'e' || peek() == 'E')) {
            ++position_;
            if (!at_end() && (peek() == '+' || peek() == '-')) {
                ++position_;
            }
            while (!at_end() && is_digit(peek())) {
                ++position_;
            }
        }

        const std::string token(text_.substr(start, position_ - start));
        char* end = nullptr;
        const double parsed = std::strtod(token.c_str(), &end);
        if (end == nullptr || *end != '\0') {
            return Error{ErrorCode::malformed, "invalid number in JSON"};
        }

        Value value;
        value.kind_ = Kind::number;
        value.number_ = parsed;
        return value;
    }

    Result<std::string> parse_string() {
        if (peek() != '"') {
            return Error{ErrorCode::malformed, "expected string"};
        }
        ++position_;

        std::string out;
        while (true) {
            if (at_end()) {
                return Error{ErrorCode::truncated, "unterminated JSON string"};
            }
            const char c = text_[position_++];
            if (c == '"') {
                return out;
            }
            if (c != '\\') {
                out.push_back(c);
                continue;
            }
            if (at_end()) {
                return Error{ErrorCode::truncated, "unterminated escape in JSON string"};
            }
            const char escape = text_[position_++];
            switch (escape) {
                case '"':
                case '\\':
                case '/':
                    out.push_back(escape);
                    break;
                case 'b':
                    out.push_back('\b');
                    break;
                case 'f':
                    out.push_back('\f');
                    break;
                case 'n':
                    out.push_back('\n');
                    break;
                case 'r':
                    out.push_back('\r');
                    break;
                case 't':
                    out.push_back('\t');
                    break;
                case 'u': {
                    if (position_ + 4 > text_.size()) {
                        return Error{ErrorCode::truncated, "truncated \\u escape in JSON string"};
                    }
                    std::uint32_t code_point = 0;
                    for (int i = 0; i < 4; ++i) {
                        const int digit = hex_digit(text_[position_ + static_cast<std::size_t>(i)]);
                        if (digit < 0) {
                            return Error{ErrorCode::malformed, "invalid \\u escape in JSON string"};
                        }
                        code_point = (code_point << 4) | static_cast<std::uint32_t>(digit);
                    }
                    position_ += 4;

                    if (code_point >= 0xD800u && code_point <= 0xDBFFu) {
                        // Surrogate pair: both halves are written as separate escapes.
                        if (position_ + 6 > text_.size() || text_[position_] != '\\' ||
                            text_[position_ + 1] != 'u') {
                            return Error{ErrorCode::malformed, "lone high surrogate in JSON string"};
                        }
                        std::uint32_t low = 0;
                        for (int i = 0; i < 4; ++i) {
                            const int digit = hex_digit(text_[position_ + 2 + static_cast<std::size_t>(i)]);
                            if (digit < 0) {
                                return Error{ErrorCode::malformed, "invalid surrogate escape"};
                            }
                            low = (low << 4) | static_cast<std::uint32_t>(digit);
                        }
                        if (low < 0xDC00u || low > 0xDFFFu) {
                            return Error{ErrorCode::malformed, "invalid low surrogate in JSON string"};
                        }
                        position_ += 6;
                        code_point = 0x10000u + ((code_point - 0xD800u) << 10) + (low - 0xDC00u);
                    } else if (code_point >= 0xDC00u && code_point <= 0xDFFFu) {
                        return Error{ErrorCode::malformed, "lone low surrogate in JSON string"};
                    }

                    append_utf8(out, code_point);
                    break;
                }
                default:
                    return Error{ErrorCode::malformed, "unknown escape in JSON string"};
            }
        }
    }

    Result<Value> parse_array(int depth) {
        ++position_;  // '['
        Value value;
        value.kind_ = Kind::array;

        skip_whitespace();
        if (!at_end() && peek() == ']') {
            ++position_;
            return value;
        }

        while (true) {
            auto item = parse_value(depth + 1);
            if (!item) {
                return item.error();
            }
            value.items_.push_back(std::move(item).take());
            skip_whitespace();
            if (at_end()) {
                return Error{ErrorCode::truncated, "unterminated JSON array"};
            }
            if (peek() == ',') {
                ++position_;
                continue;
            }
            if (peek() == ']') {
                ++position_;
                return value;
            }
            return Error{ErrorCode::malformed, "expected ',' or ']' in JSON array"};
        }
    }

    Result<Value> parse_object(int depth) {
        ++position_;  // '{'
        Value value;
        value.kind_ = Kind::object;

        skip_whitespace();
        if (!at_end() && peek() == '}') {
            ++position_;
            return value;
        }

        while (true) {
            skip_whitespace();
            if (at_end() || peek() != '"') {
                return Error{ErrorCode::malformed, "expected string key in JSON object"};
            }
            auto key = parse_string();
            if (!key) {
                return key.error();
            }
            skip_whitespace();
            if (at_end() || peek() != ':') {
                return Error{ErrorCode::malformed, "expected ':' in JSON object"};
            }
            ++position_;

            auto member = parse_value(depth + 1);
            if (!member) {
                return member.error();
            }
            value.members_.emplace_back(std::move(key).take(), std::move(member).take());

            skip_whitespace();
            if (at_end()) {
                return Error{ErrorCode::truncated, "unterminated JSON object"};
            }
            if (peek() == ',') {
                ++position_;
                continue;
            }
            if (peek() == '}') {
                ++position_;
                return value;
            }
            return Error{ErrorCode::malformed, "expected ',' or '}' in JSON object"};
        }
    }

    std::string_view text_;
    Limits limits_;
    std::size_t position_ = 0;
};

std::int64_t Value::as_int64() const noexcept {
    if (kind_ != Kind::number) {
        return 0;
    }
    // Reject anything that is not exactly representable as a 64 bit integer.
    if (!std::isfinite(number_) || number_ < -9.2e18 || number_ > 9.2e18) {
        return 0;
    }
    const auto truncated = static_cast<std::int64_t>(number_);
    if (static_cast<double>(truncated) != number_) {
        return 0;
    }
    return truncated;
}

const Value* Value::find(std::string_view key) const {
    for (const auto& [name, value] : members_) {
        if (name == key) {
            return &value;
        }
    }
    return nullptr;
}

std::size_t Value::size() const noexcept {
    return kind_ == Kind::array ? items_.size() : members_.size();
}

Result<Value> parse(std::string_view text, Limits limits) {
    Parser parser(text, limits);
    return parser.run();
}

}  // namespace velnoria::json
