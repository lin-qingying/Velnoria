#include "lumenvox/core/utf8.hpp"

#include <cstdint>

namespace lumenvox::utf8 {

bool validate_and_count(std::string_view text, std::size_t& units) {
    units = 0;
    std::size_t index = 0;

    while (index < text.size()) {
        const auto lead = static_cast<std::uint8_t>(text[index]);
        std::size_t width = 0;
        std::uint32_t code_point = 0;
        std::uint32_t minimum = 0;

        if (lead < 0x80u) {
            width = 1;
            code_point = lead;
            minimum = 0;
        } else if ((lead & 0xE0u) == 0xC0u) {
            width = 2;
            code_point = lead & 0x1Fu;
            minimum = 0x80u;
        } else if ((lead & 0xF0u) == 0xE0u) {
            width = 3;
            code_point = lead & 0x0Fu;
            minimum = 0x800u;
        } else if ((lead & 0xF8u) == 0xF0u) {
            width = 4;
            code_point = lead & 0x07u;
            minimum = 0x10000u;
        } else {
            return false;
        }

        if (index + width > text.size()) {
            return false;
        }
        for (std::size_t i = 1; i < width; ++i) {
            const auto continuation = static_cast<std::uint8_t>(text[index + i]);
            if ((continuation & 0xC0u) != 0x80u) {
                return false;
            }
            code_point = (code_point << 6) | (continuation & 0x3Fu);
        }

        if (code_point < minimum) {
            return false;  // overlong encoding
        }
        if (code_point > 0x10FFFFu || (code_point >= 0xD800u && code_point <= 0xDFFFu)) {
            return false;  // outside Unicode or a lone surrogate
        }

        units += code_point >= 0x10000u ? 2u : 1u;
        index += width;
    }

    return true;
}

}  // namespace lumenvox::utf8
