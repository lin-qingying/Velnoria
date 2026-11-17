#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace lumenvox {

// Append-only writer over a caller owned vector. Ownership stays outside so the
// same buffer can be reused across packets without reallocation churn.
class ByteWriter {
public:
    explicit ByteWriter(std::vector<std::uint8_t>& out) : out_(out) {}

    void write_u8(std::uint8_t value) { out_.push_back(value); }
    void write_bytes(std::span<const std::uint8_t> bytes) {
        out_.insert(out_.end(), bytes.begin(), bytes.end());
    }

    [[nodiscard]] std::size_t size() const noexcept { return out_.size(); }

private:
    std::vector<std::uint8_t>& out_;
};

}  // namespace lumenvox
