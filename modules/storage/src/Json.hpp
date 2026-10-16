#pragma once

// JSON mapping of the core's plain data types and helpers shared by all file formats of this
// module.

#include "readycad/core/ConstructionRules.hpp"
#include "readycad/core/Hardware.hpp"
#include "readycad/core/Material.hpp"
#include "readycad/core/Math.hpp"
#include "readycad/core/model/Carcass.hpp"
#include "readycad/core/model/Drawer.hpp"
#include "readycad/core/model/Kitchen.hpp"
#include "readycad/core/model/Product.hpp"
#include "readycad/storage/Storage.hpp"

#include <nlohmann/json.hpp>

#include <string>
#include <string_view>

// =============================================================================================
// JSON mapping of the plain data types. The macros generate to_json/from_json; they must live
// in the types' namespace so that nlohmann finds them (argument-dependent lookup).
// "_WITH_DEFAULT": a field missing in the file keeps its default value.
// =============================================================================================

namespace rc::core {

NLOHMANN_JSON_SERIALIZE_ENUM(MaterialType, {
    {MaterialType::Chipboard, "chipboard"},
    {MaterialType::MDF, "mdf"},
    {MaterialType::HDF, "hdf"},
    {MaterialType::Plywood, "plywood"},
})
NLOHMANN_JSON_SERIALIZE_ENUM(HandleType, {
    {HandleType::Bar, "bar"},
    {HandleType::Knob, "knob"},
})
NLOHMANN_JSON_SERIALIZE_ENUM(ConstructionRules::JointFastener, {
    {ConstructionRules::JointFastener::Confirmat, "confirmat"},
    {ConstructionRules::JointFastener::Eccentric, "eccentric"},
    {ConstructionRules::JointFastener::Dowel, "dowel"},
})
NLOHMANN_JSON_SERIALIZE_ENUM(JointType, {
    {JointType::Overlay, "overlay"},
    {JointType::Inset, "inset"},
})
NLOHMANN_JSON_SERIALIZE_ENUM(BackMountType, {
    {BackMountType::Groove, "groove"},
    {BackMountType::Overlay, "overlay"},
    {BackMountType::Inset, "inset"},
})
NLOHMANN_JSON_SERIALIZE_ENUM(ShelfMount, {
    {ShelfMount::Supports, "supports"},
    {ShelfMount::Confirmat, "confirmat"},
})
NLOHMANN_JSON_SERIALIZE_ENUM(HingeSide, {
    {HingeSide::Left, "left"},
    {HingeSide::Right, "right"},
})
NLOHMANN_JSON_SERIALIZE_ENUM(HandleOrientation, {
    {HandleOrientation::Vertical, "vertical"},
    {HandleOrientation::Horizontal, "horizontal"},
})
NLOHMANN_JSON_SERIALIZE_ENUM(HandleAnchor, {
    {HandleAnchor::Top, "top"},
    {HandleAnchor::Center, "center"},
    {HandleAnchor::Bottom, "bottom"},
})
NLOHMANN_JSON_SERIALIZE_ENUM(CarcassTop, {
    {CarcassTop::Panel, "panel"},
    {CarcassTop::FlatRails, "flatRails"},
    {CarcassTop::UprightRails, "uprightRails"},
})
NLOHMANN_JSON_SERIALIZE_ENUM(CutoutKind, {
    {CutoutKind::Sink, "sink"},
    {CutoutKind::Hob, "hob"},
})
NLOHMANN_JSON_SERIALIZE_ENUM(EdgeSide, {
    {EdgeSide::L1, "L1"},
    {EdgeSide::L2, "L2"},
    {EdgeSide::W1, "W1"},
    {EdgeSide::W2, "W2"},
})
NLOHMANN_JSON_SERIALIZE_ENUM(PanelCorner, {
    {PanelCorner::L1W1, "L1W1"},
    {PanelCorner::L1W2, "L1W2"},
    {PanelCorner::L2W2, "L2W2"},
    {PanelCorner::L2W1, "L2W1"},
})
NLOHMANN_JSON_SERIALIZE_ENUM(Axis, {
    {Axis::X, "x"},
    {Axis::Y, "y"},
    {Axis::Z, "z"},
})

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Material, name, type, thickness, hasGrain, decor)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Decor, id, name, texture, r, g, b, grain)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(EdgeBand, thickness, name)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Size3D, width, height, depth)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(HandleModel, id, name, supplier, article, type,
                                                holeSpacing, holeDiameter, length, width,
                                                projection)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(HandlePlacement, modelId, orientation, anchor,
                                                edgeOffset, anchorOffset)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(WorktopCutout, kind, width, depth, radius,
                                                frontOffset)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(CornerNotch, corner, alongLength, alongWidth)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(CornerRound, corner, radius)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(CornerChamfer, corner, alongLength, alongWidth)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(EdgeNotch, edge, fromEnd, offset, length, depth)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(InnerCutout, from, alongLength, alongWidth,
                                                length, width, radius)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(RoundHole, from, alongLength, alongWidth,
                                                diameter)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Kitchen::Sizes, baseHeight, wallHeight, wallDepth,
                                                worktopDepth)

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ConstructionRules::System32, pitch,
                                                frontRowOffset, rearRowMinOffset, snapShelves)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ConstructionRules::Joints, fastener,
                                                alignmentDowels, endOffset, maxSpacing)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ConstructionRules::Confirmat, faceDiameter,
                                                edgeDiameter, length)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ConstructionRules::Dowel, diameter, length,
                                                faceDepth)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ConstructionRules::Eccentric, camDiameter,
                                                camDepth, camDistance, channelDiameter,
                                                boltHoleDiameter, boltHoleDepth)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ConstructionRules::ShelfSupport, diameter, depth,
                                                holeBelowShelf, extraHoles)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ConstructionRules::Hinge, cupDiameter, cupDepth,
                                                cupEdgeDistance, endOffset, plateHoleDiameter,
                                                plateHoleDepth, plateHoleSpacing, heightLimits)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ConstructionRules::Handles, catalog,
                                                defaultDoorHandle, defaultDrawerHandle,
                                                drillOnMachine, minEdgeDistance)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ConstructionRules::Cutting, premillAllowance,
                                                rounding, edgeTrimAllowance)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ConstructionRules::Kitchen, plinthHeight,
                                                plinthSetback, railWidth, cutoutMargin)

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
    ConstructionRules, carcassMaterial, backMaterial, frontMaterial, drawerBoxMaterial,
    drawerBottomMaterial, worktopMaterial, decors, visibleEdge, hiddenEdge, backGrooveDepth, backGrooveOffset,
    backGrooveWidth, backExpansionGap, backOverlayInset, shelfSideGap, shelfFrontSetback,
    shelfBackGap, confirmatShelfFrontSetback, confirmatShelfBackGap, partitionFrontSetback,
    frontEdgeGap, frontGap, drawerSlideClearance, drawerBoxBottomClearance, drawerBoxTopClearance,
    drawerBoxMaxHeight, drawerRearClearance, drawerSlideLengths, kitchen, system32, joints, confirmat,
    dowel, eccentric, shelfSupport, hinge, minWallBetweenHoles, minBridge, handles, cutting)

} // namespace rc::core

namespace rc::storage::detail {

inline nlohmann::json parse(std::string_view text)
{
    try {
        return nlohmann::json::parse(text);
    } catch (const nlohmann::json::exception& e) {
        throw StorageError(std::string("Invalid JSON: ") + e.what());
    }
}

/// Every file starts with {"format": "readycad-...", "version": n}.
inline void checkHeader(const nlohmann::json& doc, std::string_view format, int supportedVersion)
{
    if (!doc.is_object() || doc.value("format", std::string{}) != format) {
        throw StorageError("Not a " + std::string(format) + " file");
    }
    const int version = doc.value("version", 0);
    if (version < 1 || version > supportedVersion) {
        throw StorageError("Unsupported file version " + std::to_string(version));
    }
}

} // namespace rc::storage::detail
