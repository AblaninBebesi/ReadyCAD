#pragma once

#include "readycad/core/model/Product.hpp"
#include "readycad/detailing/CutList.hpp"
#include "readycad/detailing/FlatPart.hpp"
#include "readycad/export/Drawing.hpp"
#include "readycad/nesting/Nesting.hpp"

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace rc::exporting {

/// Layers of sheet layouts (stage 7), next to the part layers of PartDrawing.hpp.
namespace layers {
inline constexpr std::string_view kSheet = "SHEET";     ///< outline of the sheet
inline constexpr std::string_view kRemnant = "REMNANT"; ///< reusable offcuts
inline constexpr std::string_view kSawCut = "SAW_CUT";  ///< panel saw cuts, in their order
inline constexpr std::string_view kPartFill = "PART_FILL"; ///< PDF only: parts shaded
} // namespace layers

/// Where the parts of a cut list come from: their part drawings and names. Without it the
/// parts are drawn as plain outlines with their labels.
using detailing::PartSource;

struct SheetDrawingOptions {
    /// Height of texts in drawing units (mm of the sheet): 20 for a DXF at 1:1, paper height /
    /// scale for a PDF page.
    Real textHeight = 20.0;
    /// Every part with its holes, grooves and edge marks (DXF for CAM); otherwise only the
    /// outline of the sawn blank and a label (cutting map for the saw operator).
    bool partDetails = true;
    bool fills = false;      ///< shade the parts (PDF)
    bool sawCuts = true;     ///< the cuts of the panel saw
    std::string title;       ///< written above the sheet; empty = none
};

/// One sheet layout at 1:1, the sheet's lower-left corner at the origin: the sheet, every part
/// turned and moved to its place, remnants with their sizes, saw cuts.
[[nodiscard]] Drawing drawSheetLayout(const nesting::NestingResult& result, std::size_t layout,
                                      const PartSource* parts,
                                      const SheetDrawingOptions& options = {});

/// Name of a part on a sheet: the cut list name, or its label.
[[nodiscard]] std::string placedPartName(const nesting::NestingResult& result,
                                         std::size_t part, const PartSource* parts);

/// Short title of a layout: "Levha 2 / 5 · Suntalam 18 · 2800 × 2100 · ×2".
[[nodiscard]] std::string sheetTitle(const nesting::NestingResult& result, std::size_t layout);

/// "02_Suntalam_18_2800x2100_x2.dxf"
[[nodiscard]] std::string sheetFileName(const nesting::NestingResult& result, std::size_t layout);

/// One DXF per sheet layout in `folder` (created if missing). Returns the written files.
/// Throws ExportError.
std::vector<std::filesystem::path> saveSheetDxfs(const nesting::NestingResult& result,
                                                 const PartSource* parts,
                                                 const std::filesystem::path& folder,
                                                 const SheetDrawingOptions& options = {});

} // namespace rc::exporting
