#pragma once

#include "readycad/core/model/Product.hpp"

#include <string>
#include <vector>

namespace rc::detailing {

/// Something the drilling could not do. Kept as data, put into words when shown (warningText):
/// the words follow the language the program speaks then.
struct DetailingWarning {
    enum class Kind {
        NoConnectorPlace, ///< a joint without room for one of its connectors
        NoShelfWall,      ///< no wall to drill a shelf's supports into
        NoHingeWall,      ///< no wall behind a door's hinge edge
        UnknownHandle,    ///< a handle model the catalogue does not have
        HandleDoesNotFit, ///< a handle off its front, or its holes too near an edge
    };
    Kind kind = Kind::NoConnectorPlace;
    core::ComponentKind part = core::ComponentKind::Product; ///< the kind of part concerned
    std::string detail;                                      ///< a handle model, as named

    friend bool operator==(const DetailingWarning&, const DetailingWarning&) = default;
};

/// "Kulp parçaya sığmıyor: Çekmece kapağı (Kulp 256)", in the current language.
[[nodiscard]] std::string warningText(const DetailingWarning& warning);

struct DetailingReport {
    int joints = 0;
    int drillings = 0;
    int grooves = 0;
    std::vector<DetailingWarning> warnings;
};

/// Generates all machining of the product's panels (присадка):
///
///  * joints     — confirmats / eccentrics / dowels for every edge-to-face joint;
///  * shelves    — shelf-support holes under every shelf on supports (System 32 rows);
///  * doors      — 35 mm hinge cups in the doors, mounting-plate holes in the walls, on the grid;
///  * back panel — grooves in sides, top and bottom when the back is mounted in a groove;
///  * handles    — screw holes of bars and knobs (unless the workshop drills them on site).
///
/// Holes and grooves are derived data: every relayout clears them, so call this after changes
/// (the editor does it automatically). Calling it twice gives the same result.
DetailingReport applyDetailing(core::Product& product);

} // namespace rc::detailing
