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

/// 2: kitchens (a run of cabinets with rails, plinths and a worktop). 3: shapes of parts
/// (notches, shaped corners, openings). 4: decors of materials. 5: corner kitchens (runs on the
/// side walls, blind and L corner cabinets). Older files still load.
inline constexpr int kProjectFormatVersion = 5;
/// 2: decors.
inline constexpr int kProfileFormatVersion = 2;
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
    std::string language = "tr";  ///< of the program: "tr", "ru", "en" (i18n::languageCode)
    std::string documentLanguage; ///< of the documents; empty: the program's
};

[[nodiscard]] std::string saveSettings(const AppSettings& settings);
/// Missing fields keep their defaults; throws StorageError for broken files or invalid values.
[[nodiscard]] AppSettings loadSettings(std::string_view json);

// --- The workshop's files ---------------------------------------------------------------------

/// The stock and the settings of a workshop, two files in one folder.
struct WorkshopFiles {
    std::filesystem::path stock;    ///< stock.rcstock
    std::filesystem::path settings; ///< settings.json

    /// The files in `folder`; empty: in userDataFolder(). Another folder serves a demo, a test
    /// or a second workshop without touching the user's own stock.
    [[nodiscard]] static WorkshopFiles in(const std::filesystem::path& folder = {});
};

/// No file yet: an empty stock. Throws StorageError for a file that cannot be read.
[[nodiscard]] nesting::StockManager loadStockFile(const std::filesystem::path& file);
/// No file yet: the default settings. Throws StorageError for a file that cannot be read.
[[nodiscard]] AppSettings loadSettingsFile(const std::filesystem::path& file);
/// Create the folder if needed and replace the file in one step (writeTextFile).
void saveStockFile(const nesting::StockManager& stock, const std::filesystem::path& file);
void saveSettingsFile(const AppSettings& settings, const std::filesystem::path& file);

// --- Files (UTF-8 paths) ----------------------------------------------------------------------

/// Writes to "<path>.tmp" and then renames it over `path`: a crash or a full disk while writing
/// never leaves a half-written project or stock file behind.
void writeTextFile(const std::filesystem::path& path, std::string_view content);
[[nodiscard]] std::string readTextFile(const std::filesystem::path& path);

/// Where files of the user (not of a project) live: %APPDATA%\ReadyCAD on Windows,
/// $XDG_CONFIG_HOME/ReadyCAD or ~/.config/ReadyCAD elsewhere. Not created here.
[[nodiscard]] std::filesystem::path userDataFolder();

} // namespace rc::storage
