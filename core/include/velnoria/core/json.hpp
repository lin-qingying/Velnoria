#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "velnoria/core/error.hpp"

// Minimal JSON reader for the data pipeline and for the status response.
//
// Scope is deliberate: no comments, no trailing commas, no extensions. Object
// member order is preserved because some reports are re-emitted with the same
// order they were read, and sorting would be a silent behavioural change.
//
// Depth and size are bounded for the same reason NBT is: the input comes from
// another process or the network.

namespace velnoria::json {

enum class Kind : std::uint8_t { null_value, boolean, number, string, array, object };

class Value {
public:
    Value() = default;

    [[nodiscard]] Kind kind() const noexcept { return kind_; }
    [[nodiscard]] bool is_null() const noexcept { return kind_ == Kind::null_value; }
    [[nodiscard]] bool is_object() const noexcept { return kind_ == Kind::object; }
    [[nodiscard]] bool is_array() const noexcept { return kind_ == Kind::array; }
    [[nodiscard]] bool is_string() const noexcept { return kind_ == Kind::string; }
    [[nodiscard]] bool is_number() const noexcept { return kind_ == Kind::number; }

    [[nodiscard]] bool as_bool() const noexcept { return boolean_; }
    [[nodiscard]] double as_number() const noexcept { return number_; }
    [[nodiscard]] std::int64_t as_int64() const noexcept;
    [[nodiscard]] const std::string& as_string() const noexcept { return string_; }
    [[nodiscard]] const std::vector<Value>& items() const noexcept { return items_; }
    [[nodiscard]] const std::vector<std::pair<std::string, Value>>& members() const noexcept {
        return members_;
    }

    [[nodiscard]] const Value* find(std::string_view key) const;
    [[nodiscard]] std::size_t size() const noexcept;

    friend class Parser;

private:
    Kind kind_ = Kind::null_value;
    bool boolean_ = false;
    double number_ = 0.0;
    std::string string_;
    std::vector<Value> items_;
    std::vector<std::pair<std::string, Value>> members_;
};

struct Limits {
    int max_depth = 64;
    std::size_t max_bytes = 64u * 1024u * 1024u;
};

[[nodiscard]] Result<Value> parse(std::string_view text, Limits limits = {});

}  // namespace velnoria::json
