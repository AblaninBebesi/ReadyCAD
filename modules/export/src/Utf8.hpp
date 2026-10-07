#pragma once

// Small UTF-8 helpers private to the export module.

#include <filesystem>
#include <string>
#include <string_view>

namespace rc::exporting {

/// Code points of a UTF-8 string; invalid bytes become U+FFFD.
[[nodiscard]] inline std::u32string decodeUtf8(std::string_view text)
{
    std::u32string out;
    out.reserve(text.size());
    std::size_t i = 0;
    while (i < text.size()) {
        const auto byte = [&](std::size_t k) { return static_cast<unsigned char>(text[k]); };
        const unsigned char lead = byte(i);
        std::size_t extra = 0;
        char32_t code = 0;
        if (lead < 0x80) {
            code = lead;
        } else if ((lead & 0xE0U) == 0xC0U) {
            extra = 1;
            code = lead & 0x1FU;
        } else if ((lead & 0xF0U) == 0xE0U) {
            extra = 2;
            code = lead & 0x0FU;
        } else if ((lead & 0xF8U) == 0xF0U) {
            extra = 3;
            code = lead & 0x07U;
        } else {
            out += U'�';
            ++i;
            continue;
        }
        if (i + extra >= text.size()) { // truncated sequence at the end
            out += U'�';
            break;
        }
        bool valid = true;
        for (std::size_t k = 1; k <= extra; ++k) {
            if ((byte(i + k) & 0xC0U) != 0x80U) {
                valid = false;
                break;
            }
            code = (code << 6) | (byte(i + k) & 0x3FU);
        }
        out += valid ? code : U'�';
        i += valid ? extra + 1 : 1;
    }
    return out;
}

/// A path as UTF-8 (std::filesystem::path::string() uses the ANSI code page on Windows).
[[nodiscard]] inline std::string pathToUtf8(const std::filesystem::path& path)
{
    const std::u8string u8 = path.u8string();
    return {u8.begin(), u8.end()};
}

} // namespace rc::exporting
