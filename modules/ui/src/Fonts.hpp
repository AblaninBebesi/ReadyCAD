#pragma once

namespace rc::ui {

/// Loads a UI font that contains the Turkish letters (ç ğ ı İ ö ş ü).
/// ImGui's built-in font is ASCII-only, so we look for a system TrueType font and fall back to the
/// built-in one (Turkish letters then show as '?') if none is found.
/// Returns true if a font with Turkish glyphs was loaded.
bool loadUiFonts(float sizePixels);

} // namespace rc::ui
