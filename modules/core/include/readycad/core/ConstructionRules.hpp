#pragma once

#include "readycad/core/Decor.hpp"
#include "readycad/core/Hardware.hpp"
#include "readycad/core/Material.hpp"

#include <string>
#include <vector>

namespace rc::core {

/// The workshop profile: all technological parameters ("заводские нормы") plus the workshop's
/// own hardware catalogue (handles of its supplier).
///
/// The formulas live in the component classes (each part knows HOW it is sized), the numbers and
/// catalogues live here (a workshop can tune them without touching code). One instance belongs
/// to a Product; stage 5 saves/loads it as a file per workshop.
struct ConstructionRules {
    // --- Materials --------------------------------------------------------------------------
    Material carcassMaterial{"Suntalam 18", MaterialType::Chipboard, 18.0, true, "wood-beige"};
    Material backMaterial{"HDF 3", MaterialType::HDF, 3.0, false, {}};
    Material frontMaterial{"MDF Lam 18", MaterialType::MDF, 18.0, false, "white"};
    Material drawerBoxMaterial{"Suntalam 16", MaterialType::Chipboard, 16.0, true, {}};
    Material drawerBottomMaterial{"HDF 3", MaterialType::HDF, 3.0, false, {}};
    Material worktopMaterial{"Tezgah 38", MaterialType::Chipboard, 38.0, false, "marble-grey"};

    /// The decors of the workshop's supplier (the materials above refer to them).
    std::vector<Decor> decors = defaultDecorCatalog();

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
    Real shelfSideGap = 1.0;       // shelf on supports: per side, so it drops in between the walls
    Real shelfFrontSetback = 20.0; // shelf on supports: behind the carcass front edge
    Real shelfBackGap = 2.0;       // shelf on supports: in front of the back panel
    Real confirmatShelfFrontSetback = 0.0; // shelf on confirmats: behind the carcass front edge
    Real confirmatShelfBackGap = 0.0;      // shelf on confirmats: in front of the back panel
    Real partitionFrontSetback = 0.0;      // partitions and fixed (dividing) shelves

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

    // --- Kitchens ---------------------------------------------------------------------------
    struct Kitchen {
        Real plinthHeight = 100.0; // adjustable legs under base cabinets, hidden by the plinth
        Real plinthSetback = 40.0; // plinth face behind the carcass front edge: room for the feet
        Real railWidth = 100.0;    // kuşak: depth of a flat rail, height of an upright one
        Real cutoutMargin = 50.0;  // worktop left around a sink or hob cut-out, to the edges
    } kitchen;

    // --- System 32 --------------------------------------------------------------------------
    /// All holes of a carcass (shelf supports, hinge plates) lie on one vertical grid with a
    /// 32 mm pitch. Its origin is the centre plane of the bottom panel.
    struct System32 {
        Real pitch = 32.0;
        Real frontRowOffset = 37.0;   // hole row from the front edge of a wall
        Real rearRowMinOffset = 37.0; // rear row at least this far from the shelf's rear edge
        bool snapShelves = true;      // shelves on supports stand exactly on a hole
    } system32;

    // --- Fasteners (stage 4 drilling) -------------------------------------------------------
    enum class JointFastener { Confirmat, Eccentric, Dowel };

    /// Panel-to-panel joints of the carcass and the drawer boxes.
    struct Joints {
        JointFastener fastener = JointFastener::Confirmat;
        bool alignmentDowels = false; // extra dowel 32 mm next to every connector
        Real endOffset = 50.0;        // first/last connector from the end of the joint
        Real maxSpacing = 300.0;      // max distance between neighbouring connectors
    } joints;

    struct Confirmat { // евровинт 7x50
        Real faceDiameter = 7.0; // through hole in the face panel
        Real edgeDiameter = 5.0; // hole in the edge of the other panel
        Real length = 50.0;
    } confirmat;

    struct Dowel { // шкант 8x30
        Real diameter = 8.0;
        Real length = 30.0;
        Real faceDepth = 12.0; // blind hole in the face panel; the rest goes into the edge
    } dowel;

    struct Eccentric { // эксцентрик (minifix) 15 mm
        Real camDiameter = 15.0;
        Real camDepth = 12.5;
        Real camDistance = 34.0;     // cam centre from the edge of its panel
        Real channelDiameter = 8.0;  // channel for the bolt in that edge
        Real boltHoleDiameter = 5.0; // the bolt screws into the face panel
        Real boltHoleDepth = 11.0;
    } eccentric;

    struct ShelfSupport { // полкодержатель
        Real diameter = 5.0;
        Real depth = 10.0;
        Real holeBelowShelf = 2.5; // shelf underside above the hole centre (pin radius)
        int extraHoles = 0;        // additional holes above and below, to re-position later
    } shelfSupport;

    struct Hinge { // cup hinge with a mounting plate, 35 mm cup
        Real cupDiameter = 35.0;
        Real cupDepth = 13.0;
        Real cupEdgeDistance = 22.5; // cup centre from the door's hinge edge
        Real endOffset = 100.0;      // first/last hinge from the door's top/bottom edge
        Real plateHoleDiameter = 5.0;
        Real plateHoleDepth = 10.0;
        Real plateHoleSpacing = 32.0;                // vertical, on the System 32 grid
        std::vector<Real> heightLimits{900, 1600, 2000}; // more hinges above each limit
    } hinge;

    /// Two blind holes from opposite faces (e.g. shelf supports on both sides of a partition)
    /// must leave at least this much material between them.
    Real minWallBetweenHoles = 1.0;

    /// A cut-out the user adds (an opening, a cable hole) keeps at least this much material to
    /// the outline of the part and to the other openings.
    Real minBridge = 10.0;

    // --- Handles ----------------------------------------------------------------------------
    struct Handles {
        std::vector<HandleModel> catalog = defaultHandleCatalog();
        std::string defaultDoorHandle = "bar-128";
        std::string defaultDrawerHandle = "bar-128";
        /// true: the CNC drills the screw holes; false: the installer drills them on site
        /// (the handles still appear in the hardware list and in 3D).
        bool drillOnMachine = true;
        Real minEdgeDistance = 10.0; // from a hole to the front's edge

        [[nodiscard]] const HandleModel* find(const std::string& id) const noexcept
        {
            for (const HandleModel& model : catalog) {
                if (model.id == id) {
                    return &model;
                }
            }
            return nullptr;
        }
    } handles;

    // --- Cutting ----------------------------------------------------------------------------
    struct Cutting {
        Real premillAllowance = 1.0; // per banded edge: the edgebander mills it off first
        Real rounding = 0.1;         // cut sizes are rounded to this step
        Real edgeTrimAllowance = 30.0; // extra edge banding per edge piece (trimmed off)
    } cutting;

    /// Shared immutable instance with the defaults above.
    [[nodiscard]] static const ConstructionRules& defaults();
};

} // namespace rc::core
