#pragma once

#include "readycad/core/model/Product.hpp"
#include "readycad/export/Drawing.hpp"

namespace rc::exporting {

/// Orthographic views of the whole product for the assembly drawing (first-angle projection,
/// ISO 128 / ISO 5456-2, as used in Turkey and Europe).
enum class ProductView {
    Front,     ///< from the front, with doors, drawer fronts and handles
    FrontOpen, ///< from the front without fronts: shelves, partitions, drawer boxes; trace of A-A
    Section,   ///< section A-A through the first compartment, seen from the left
    /// From above, the whole product (a kitchen without its wall cabinets): the base cabinets
    /// under the worktops, the worktops' joints and cut-outs, the lengths of the walls. The back
    /// wall at the top of the drawing, the room below it.
    Plan,
};

struct ProductViewOptions {
    /// Text height in drawing units (mm of the product): on paper height / scale.
    Real textHeight = 25.0;
    bool dimensions = true;
};

/// Layer names of the views.
namespace viewLayers {
inline constexpr std::string_view kVisible = "VISIBLE";       ///< visible edges
inline constexpr std::string_view kSection = "SECTION";       ///< outlines of cut parts
inline constexpr std::string_view kSectionFill = "SECTION_FILL"; ///< cut parts, shaded
inline constexpr std::string_view kHandles = "HANDLES";
inline constexpr std::string_view kCutLine = "CUT_LINE";      ///< trace of the section plane
inline constexpr std::string_view kDimensions = "DIMENSIONS";
inline constexpr std::string_view kText = "TEXT";
} // namespace viewLayers

/// Hidden edges are removed: a panel edge is drawn only where no part entirely in front of it
/// covers it (all parts are boxes, so this is exact for furniture).
///
/// `root`: the product, or a kitchen run — its elevation, seen standing in front of its wall, in
/// its own coordinates. The plan is always that of the whole product.
[[nodiscard]] Drawing drawProductView(const core::FurnitureComponent& root, ProductView view,
                                      const ProductViewOptions& options = {});

/// Extent of the view's geometry without dimensions and texts (to choose the scale).
[[nodiscard]] Bounds viewExtent(const core::FurnitureComponent& root, ProductView view);

/// X of the section plane A-A: inside the first compartment, off its centre so that it does
/// not fall into the gap between two doors.
[[nodiscard]] Real sectionPlaneX(const core::FurnitureComponent& root);

} // namespace rc::exporting
