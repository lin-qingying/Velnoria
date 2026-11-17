#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

// SHA-1 is used for one purpose: verifying that the files we read are the exact
// ones the version manifest declares. It is not a security boundary, and the
// streaming form exists so multi gigabyte jars never have to be held in memory.

namespace velnoria {

using Digest20 = std::array<std::uint8_t, 20>;

class Sha1 {
public:
    void update(std::span<const std::uint8_t> data);
    [[nodiscard]] Digest20 final();

private:
    void process_block(const std::uint8_t* block);

    std::uint32_t state_[5] = {0x67452301u, 0xEFCDAB89u, 0x98BADCFEu, 0x10325476u, 0xC3D2E1F0u};
    std::uint8_t buffer_[64] = {};
    std::size_t buffer_size_ = 0;
    std::uint64_t total_bytes_ = 0;
    bool finished_ = false;
};

[[nodiscard]] Digest20 sha1(std::span<const std::uint8_t> data);
[[nodiscard]] Digest20 sha1(std::string_view data);

}  // namespace velnoria
