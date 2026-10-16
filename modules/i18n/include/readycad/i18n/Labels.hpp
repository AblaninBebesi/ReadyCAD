#pragma once

#include "readycad/core/Hardware.hpp"
#include "readycad/core/Panel.hpp"
#include "readycad/core/builders/Catalog.hpp"
#include "readycad/core/model/FurnitureComponent.hpp"
#include "readycad/core/model/Kitchen.hpp"
#include "readycad/core/model/Parts.hpp"
#include "readycad/i18n/Localization.hpp"

#include <optional>
#include <string>

namespace rc::i18n {

[[nodiscard]] TextId kindText(core::ComponentKind kind) noexcept;
[[nodiscard]] TextId catalogText(core::CatalogItem item) noexcept;
[[nodiscard]] TextId holeText(core::FastenerType type) noexcept;
/// "U1", "U2" (long edges), "G1", "G2" (short edges).
[[nodiscard]] TextId edgeText(core::EdgeSide side) noexcept;
[[nodiscard]] TextId hardwareText(core::HardwareKind kind) noexcept;

/// Name shown in the tree: the user's own name, or the kind plus its number among siblings of
/// the same kind ("Raf 2"), or just the kind if it is the only one.
[[nodiscard]] std::string displayName(const core::FurnitureComponent& component);

/// Name of a cut list row: the user's own name, or the kind with the hand of a mirror part:
/// "Yan panel (sol)", "Kapak (sağ)" (doors: the hinge side), "Raf".
[[nodiscard]] std::string partName(core::ComponentKind kind, const std::string& name,
                                   std::optional<core::Side> hand);

/// A corner by its edges as the drawings name them: "U1–G1".
[[nodiscard]] std::string cornerText(core::PanelCorner corner);
/// The kind of a shape feature: "Köşe kesimi", "Yuvarlak delik"...
[[nodiscard]] TextId featureKindText(const core::ShapeFeature& feature) noexcept;
/// A shape feature in one line, as the drawings name the edges:
/// "Köşe kesimi U1–G1: 20 × 100", "Kenar kesimi U2: G1'den 300, 100 × 30",
/// "Yuvarlak delik Ø60: U1–G1 köşesinden 100 / 60".
[[nodiscard]] std::string featureText(const core::ShapeFeature& feature);

/// 1982 -> "1982", 559.5 -> "559.5" (lengths in mm).
[[nodiscard]] std::string mm(core::Real value);

} // namespace rc::i18n
