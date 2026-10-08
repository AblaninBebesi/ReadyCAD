#pragma once

#include "readycad/core/ConstructionRules.hpp"
#include "readycad/cam/NestingCenter.hpp"
#include "readycad/core/model/Product.hpp"
#include "readycad/nesting/Nesting.hpp"
#include "readycad/nesting/Stock.hpp"

#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>

namespace rc::storage {

/// Any problem reading or writing a file: I/O error, invalid JSON, unknown format or version.
class StorageError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

inline constexpr int kProjectFormatVersion = 1;
inline constexpr int kProfileFormatVersion = 1;
inline constexpr int kStockFormatVersion = 1;
inline constexpr int kSettingsFormatVersion = 1;

// --- Workshop profile (*.rcprofile): ConstructionRules incl. the handle catalogue -------------

[[nodiscard]] std::string saveProfile(const core::ConstructionRules& rules);
/// Missing fields keep their defaults, so older profile files stay loadable.
[[nodiscard]] core::ConstructionRules loadProfile(std::string_view json);

// --- Project (*.rcproj): one product with the rules it was designed with ----------------------
//
// The file stores the DESIGN (dimensions, structure, per-part choices), not the computed geometry
// or drilling: those are recomputed on load, so a file never contains stale machining.

[[nodiscard]] std::string saveProduct(const core::Product& product);
/// Returns a fully laid-out product.
[[nodiscard]] std::shared_ptr<core::Product> loadProduct(std::string_view json);

// --- Stock of sheets and remnants (stock.rcstock, stage 7) -------------------------------------
//
// Belongs to the workshop, not to a project: one file in userDataFolder(), shared by all
// projects, so the remnants of one job are offered to the next.

[[nodiscard]] std::string saveStock(const nesting::StockManager& stock);
[[nodiscard]] nesting::StockManager loadStock(std::string_view json);

// --- Application settings (settings.json) -------------------------------------------------------

/// The workshop's machines: cutting and its optimiser (stage 7), the CNC nesting centre with its
/// tools and controller dialect (stage 8).
struct AppSettings {
    nesting::NestingSettings nesting;
    cam::NestingCenterProfile cnc;
};

[[nodiscard]] std::string saveSettings(const AppSettings& settings);
/// Missing fields keep their defaults; throws StorageError for broken files or invalid values.
[[nodiscard]] AppSettings loadSettings(std::string_view json);

// --- Files (UTF-8 paths) ----------------------------------------------------------------------

/// Writes to "<path>.tmp" and then renames it over `path`: a crash or a full disk while writing
/// never leaves a half-written project or stock file behind.
void writeTextFile(const std::filesystem::path& path, std::string_view content);
[[nodiscard]] std::string readTextFile(const std::filesystem::path& path);

/// Where files of the user (not of a project) live: %APPDATA%\ReadyCAD on Windows,
/// $XDG_CONFIG_HOME/ReadyCAD or ~/.config/ReadyCAD elsewhere. Not created here.
[[nodiscard]] std::filesystem::path userDataFolder();

} // namespace rc::storage
