#pragma once

#include "readycad/core/Material.hpp"

#include <vector>

namespace rc::core {

/// All technological parameters ("заводские нормы") that the layout formulas use.
///
/// The formulas live in the component classes (each part knows HOW it is sized), the numbers live
/// here (a workshop can tune them without touching code). One instance belongs to a Product.
struct ConstructionRules {
    // --- Materials --------------------------------------------------------------------------
    Material carcassMaterial{"Suntalam 18", MaterialType::Chipboard, 18.0, true};
    Material backMaterial{"HDF 3", MaterialType::HDF, 3.0, false};
    Material frontMaterial{"MDF Lam 18", MaterialType::MDF, 18.0, false};
    Material drawerBoxMaterial{"Suntalam 16", MaterialType::Chipboard, 16.0, true};
    Material drawerBottomMaterial{"HDF 3", MaterialType::HDF, 3.0, false};

    // --- Edge banding -----------------------------------------------------------------------
    EdgeBand visibleEdge{2.0, "ABS 2"};   // front edges you see and touch
    EdgeBand hiddenEdge{0.4, "PVC 0.4"}; // edges protected from moisture only

    // --- Back panel -------------------------------------------------------------------------
    Real backGrooveDepth = 8.0;    // groove depth in sides/top/bottom
    Real backGrooveOffset = 16.0;  // from the carcass rear edge to the groove
    Real backGrooveWidth = 4.0;    // must be >= back thickness
    Real backExpansionGap = 1.5;   // per side, room for humidity/temperature movement in the groove
    Real backOverlayInset = 2.0;   // nailed-on back: distance from the carcass outer edges

    // --- Shelves and partitions -------------------------------------------------------------
    Real shelfSideGap = 1.0;       // adjustable shelf: per side, so it drops in between the walls
    Real shelfFrontSetback = 20.0; // adjustable shelf: behind the carcass front edge
    Real shelfBackGap = 2.0;       // adjustable shelf: in front of the back panel
    Real partitionFrontSetback = 0.0;

    // --- Fronts (doors, drawer fronts) ------------------------------------------------------
    Real frontEdgeGap = 1.5; // from a front edge to the outer carcass edge
    Real frontGap = 3.0;     // between two neighbouring fronts

    // --- Drawers ----------------------------------------------------------------------------
    Real drawerSlideClearance = 13.0;     // per side, ball-bearing slides
    Real drawerBoxBottomClearance = 10.0; // below the drawer box inside its zone
    Real drawerBoxTopClearance = 20.0;    // above the drawer box inside its zone
    Real drawerBoxMaxHeight = 250.0;
    Real drawerRearClearance = 10.0;      // between the box and the back panel
    std::vector<Real> drawerSlideLengths{250, 300, 350, 400, 450, 500, 550, 600, 650};

    /// Shared immutable instance with the defaults above.
    [[nodiscard]] static const ConstructionRules& defaults();
};

} // namespace rc::core
