#pragma once

#include "readycad/core/builders/KitchenBuilder.hpp"
#include "readycad/core/model/Drawer.hpp"
#include "readycad/core/model/Kitchen.hpp"
#include "readycad/editor/Document.hpp"

#include <optional>
#include <string>
#include <vector>

/// Everything the user can change in the UI, as named, undoable operations on a Document.
///
/// The UI only calls these functions (it never mutates the model directly), so the whole path
/// "parameter panel -> model -> drilling" is covered by tests without a window. Components are
/// addressed by ComponentId; a wrong id or type throws std::invalid_argument, an impossible
/// change (drawers into a section with shelves) throws std::logic_error — in both cases the
/// document stays unchanged.
namespace rc::editor {

// --- Product --------------------------------------------------------------------------------
void setDimensions(Document& doc, const core::Size3D& dimensions);
void setProductName(Document& doc, std::string name);
void setRules(Document& doc, core::ConstructionRules rules);

// --- Carcass --------------------------------------------------------------------------------
void setTopJoint(Document& doc, core::ComponentId carcass, core::JointType joint);
void setBottomJoint(Document& doc, core::ComponentId carcass, core::JointType joint);
void setBackMount(Document& doc, core::ComponentId carcass, core::BackMountType mount);
/// A top panel or rails (base cabinets under a worktop).
void setTopStyle(Document& doc, core::ComponentId carcass, core::CarcassTop style);
void setPlinth(Document& doc, core::ComponentId carcass, bool plinth);

// --- Kitchens -------------------------------------------------------------------------------
void setKitchenSizes(Document& doc, const core::Kitchen::Sizes& sizes);
/// Adds a cabinet of a module at the right end of its row (base or wall), `width` mm wide.
/// The other cabinets give way; if they cannot (all fixed, too little room) the edit is refused.
void addCabinet(Document& doc, core::CabinetModule module, core::Real width);
/// nullopt: the cabinet shares the free width with the other automatic ones.
void setCabinetWidth(Document& doc, core::ComponentId cabinet, std::optional<core::Real> width);
/// Moves a cabinet of a row `steps` places to the right (negative: to the left).
void moveCabinet(Document& doc, core::ComponentId cabinet, int steps);
/// The opening a base cabinet needs in the worktop; nullopt = none.
void setCutout(Document& doc, core::ComponentId cabinet,
               std::optional<core::WorktopCutout> cutout);

// --- Sections -------------------------------------------------------------------------------
void splitSection(Document& doc, core::ComponentId section, core::Axis axis, int count);
/// Removes everything inside the section (sub-sections, shelves, doors, drawers).
void clearSection(Document& doc, core::ComponentId section);
void setSectionFixedSize(Document& doc, core::ComponentId section, std::optional<core::Real> size);
void setShelfCount(Document& doc, core::ComponentId section, int count,
                   core::ShelfMount mount = core::ShelfMount::Supports);
void setDoorCount(Document& doc, core::ComponentId section, int count);
void setDrawerCount(Document& doc, core::ComponentId section, int count);

// --- Parts ----------------------------------------------------------------------------------
void setShelfMount(Document& doc, core::ComponentId shelf, core::ShelfMount mount);
void setShelfElevation(Document& doc, core::ComponentId shelf, std::optional<core::Real> elevation);
void setHinge(Document& doc, core::ComponentId door, core::HingeSide side);
void setHandle(Document& doc, core::ComponentId front,
               std::optional<core::HandlePlacement> placement);
void setMaterialOverride(Document& doc, core::ComponentId part,
                         std::optional<core::Material> material);
/// The notches, shaped corners and openings of a part (the whole list: the panel edits a copy).
/// A shape that does not fit, or takes away material a hole or a groove needs, is refused.
void setShape(Document& doc, core::ComponentId part, std::vector<core::ShapeFeature> features);
/// Puts parts into the cutting plan or leaves them out (bought cut to size, cut elsewhere), in one
/// step — e.g. all parts of a cut list row.
void setInCuttingPlan(Document& doc, const std::vector<core::ComponentId>& parts, bool inPlan);
/// Removes a shelf, a door, a drawer stack or a cabinet of a kitchen. Structural parts cannot
/// be removed.
void removeComponent(Document& doc, core::ComponentId component);

} // namespace rc::editor
