#pragma once

#include <filesystem>

namespace rc::ui {

/// The program's own files (decor pictures; at stage 10 the font): the folder `assets` next to
/// ReadyCAD.exe — the build copies it there — or, when the program runs from its build folder
/// without it, the one in the source tree. Empty if neither exists.
[[nodiscard]] std::filesystem::path assetsFolder();

} // namespace rc::ui
