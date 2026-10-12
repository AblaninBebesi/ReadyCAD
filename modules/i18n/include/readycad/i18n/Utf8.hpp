#pragma once

// UTF-8 text helpers shared by the modules that write files for other programs (DXF, PDF,
// CNC programs): their readers often know only ASCII.

#include <filesystem>
#include <string>
#include <string_view>

namespace rc::i18n {

/// Code points of a UTF-8 string; invalid bytes become U+FFFD.
[[nodiscard]] std::u32string decodeUtf8(std::string_view text);

/// Turkish letters to their ASCII base letter ("Çekmece yanı" -> "Cekmece yani"), every other
/// non-ASCII character to '_': for file names, DXF layer names and CNC program comments.
[[nodiscard]] std::string asciiFold(std::string_view text);

/// A name that works as a file or folder name everywhere (CNC controllers, USB sticks): asciiFold,
/// then everything but letters, digits and '-' becomes '_' ("Dolap 1/2" -> "Dolap_1_2").
[[nodiscard]] std::string safeFileName(std::string_view text);

/// A path as UTF-8 (std::filesystem::path::string() uses the ANSI code page on Windows).
[[nodiscard]] std::string pathToUtf8(const std::filesystem::path& path);

} // namespace rc::i18n
