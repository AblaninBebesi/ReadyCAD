#pragma once

#include "readycad/core/model/Drawer.hpp"
#include "readycad/editor/Document.hpp"

#include <optional>
#include <string>

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
void setTopJoint(Document& doc, core::JointType joint);
void setBottomJoint(Document& doc, core::JointType joint);
void setBackMount(Document& doc, core::BackMountType mount);

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
/// Removes a shelf, a door or a drawer stack. Structural parts cannot be removed.
void removeComponent(Document& doc, core::ComponentId component);

} // namespace rc::editor
