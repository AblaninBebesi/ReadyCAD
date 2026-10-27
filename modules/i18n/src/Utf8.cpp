#include "readycad/i18n/Utf8.hpp"

namespace rc::i18n {

std::u32string decodeUtf8(std::string_view text)
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

namespace {

/// А..я (U+0410..U+044F) in Latin letters; ё apart.
constexpr const char* kCyrillic[32] = {"a", "b", "v", "g", "d", "e", "zh", "z", "i", "i", "k",
                                       "l", "m", "n", "o", "p", "r", "s", "t", "u", "f", "kh",
                                       "ts", "ch", "sh", "shch", "", "y", "", "e", "iu", "ia"};

} // namespace

std::string asciiFold(std::string_view text)
{
    std::string out;
    for (const char32_t c : decodeUtf8(text)) {
        if ((c >= 0x410 && c <= 0x44F) || c == 0x401 || c == 0x451) {
            const bool upper = c < 0x430 || c == 0x401;
            std::string latin = c == 0x401 || c == 0x451 ? "e" : kCyrillic[(c - 0x410) % 32];
            if (upper && !latin.empty()) {
                latin[0] = static_cast<char>(latin[0] - 'a' + 'A');
            }
            out += latin;
            continue;
        }
        switch (c) {
        case U'ç': out += 'c'; break;
        case U'Ç': out += 'C'; break;
        case U'ğ': out += 'g'; break;
        case U'Ğ': out += 'G'; break;
        case U'ı': out += 'i'; break;
        case U'İ': out += 'I'; break;
        case U'ö': out += 'o'; break;
        case U'Ö': out += 'O'; break;
        case U'ş': out += 's'; break;
        case U'Ş': out += 'S'; break;
        case U'ü': out += 'u'; break;
        case U'Ü': out += 'U'; break;
        case U'â': out += 'a'; break;
        case U'î': out += 'i'; break;
        case U'û': out += 'u'; break;
        default:   out += c < 0x80 ? static_cast<char>(c) : '_'; break;
        }
    }
    return out;
}

std::string safeFileName(std::string_view text)
{
    std::string name;
    for (const char c : asciiFold(text)) {
        const bool safe = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')
                       || c == '-';
        if (safe) {
            name += c;
        } else if (!name.empty() && name.back() != '_') {
            name += '_'; // a run of other characters becomes one underscore
        }
    }
    while (!name.empty() && name.back() == '_') {
        name.pop_back();
    }
    return name;
}

std::string pathToUtf8(const std::filesystem::path& path)
{
    const std::u8string u8 = path.u8string();
    return {u8.begin(), u8.end()};
}

} // namespace rc::i18n
