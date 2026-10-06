#pragma once

#include "readycad/core/ConstructionRules.hpp"
#include "readycad/core/model/Product.hpp"

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

// --- Files (UTF-8 paths) ----------------------------------------------------------------------

void writeTextFile(const std::filesystem::path& path, std::string_view content);
[[nodiscard]] std::string readTextFile(const std::filesystem::path& path);

} // namespace rc::storage
