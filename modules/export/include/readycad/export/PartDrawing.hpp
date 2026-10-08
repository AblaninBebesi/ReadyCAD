#pragma once

#include "readycad/core/model/Product.hpp"
#include "readycad/detailing/CutList.hpp"
#include "readycad/detailing/FlatPart.hpp"
#include "readycad/export/Drawing.hpp"

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace rc::exporting {

/// Layer names of part drawings: the contract with CAD/CAM software, documented in
/// docs/ARCHITECTURE.md. Holes and grooves get one layer per diameter/width and depth, e.g.
///   DRILL_D5_Z13        5 mm hole, 13 deep, in the view face (machined from above)
///   DRILL_BACK_D7_Z18   in the opposite face (the part must be turned over)
///   DRILL_EDGE_D8_Z30   horizontal hole in an edge, drawn as its outline from the entry point
///   GROOVE_W4_Z8        groove 4 wide, 8 deep (GROOVE_BACK_... on the opposite face)
/// A decimal point is written as "p": DRILL_D15_Z12p5.
namespace layers {
inline constexpr std::string_view kContour = "CONTOUR";        ///< finished part, closed
inline constexpr std::string_view kCutContour = "CUT_CONTOUR"; ///< sawn blank, closed
inline constexpr std::string_view kEdgeBand = "EDGE_BAND";     ///< marks along banded edges
inline constexpr std::string_view kText = "TEXT";
inline constexpr std::string_view kDimensions = "DIMENSIONS";
} // namespace layers

[[nodiscard]] std::string drillLayerName(detailing::FlatSide side, Real diameter, Real depth);
[[nodiscard]] std::string grooveLayerName(detailing::FlatSide side, Real width, Real depth);

struct PartDrawingOptions {
    /// Height of texts in drawing units. On paper: wanted height / scale (2.5 mm at 1:10 -> 25).
    Real textHeight = 2.5;
    bool cutContour = false; ///< also draw the sawn blank
    bool edgeBands = true;   ///< marks inside banded edges and edge names outside
    bool dimensions = true;  ///< overall length and width
    std::string title;       ///< written above the part; empty = none
};

/// The part as seen from its view face (see FlatPart), in mm, origin at the lower-left corner
/// of the finished part.
[[nodiscard]] Drawing drawPart(const detailing::FlatPart& part, const PartDrawingOptions& options);

// ---------------------------------------------------------------------------------------------
// All parts of a product
// ---------------------------------------------------------------------------------------------

/// Name of a cut list row as people read it: the user's own name or the kind ("Yan panel").
[[nodiscard]] std::string partName(const detailing::CutListRow& row);

struct PartsSheetOptions {
    Real textHeight = 20.0; ///< 1:1 drawing, so texts are big
    Real gap = 150.0;       ///< between parts
    Real rowWidth = 5000.0; ///< parts wrap into a new row after this width
    bool cutContour = true;
};

/// Every distinct part of the cut list side by side in one drawing (DXF "all parts"), each with
/// its number, name, quantity and size above it.
[[nodiscard]] Drawing drawAllParts(const core::Product& product,
                                   const std::vector<detailing::CutListRow>& cutList,
                                   const PartsSheetOptions& options = {});

/// "03_Raf_762x518.dxf": number in the cut list, ASCII name, cut size.
[[nodiscard]] std::string partFileName(std::size_t number, const detailing::CutListRow& row);

/// One DXF per distinct part in `folder` (created if missing). Returns the written files.
/// Throws ExportError.
std::vector<std::filesystem::path> savePartDxfs(const core::Product& product,
                                                const std::vector<detailing::CutListRow>& cutList,
                                                const std::filesystem::path& folder,
                                                const PartsSheetOptions& options = {});

} // namespace rc::exporting
