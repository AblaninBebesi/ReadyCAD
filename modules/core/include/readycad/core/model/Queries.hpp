#pragma once

#include "readycad/core/model/FurnitureComponent.hpp"

#include <vector>

namespace rc::core {

/// Every component in the subtree that owns a physical panel (pre-order).
/// This is what detailing, cut lists, nesting and CAM work with.
[[nodiscard]] std::vector<const FurnitureComponent*> collectPanels(const FurnitureComponent& root);

/// Union of all panel boxes of the subtree, in product coordinates (empty if there are none).
[[nodiscard]] Aabb panelBounds(const FurnitureComponent& root);

struct Overlap {
    const FurnitureComponent* first = nullptr;
    const FurnitureComponent* second = nullptr;
    Aabb region; // common volume, product coordinates
};

/// Pairs of panels whose volumes intersect (touching is fine), except legitimate cases such as
/// a back panel sitting in its groove. An empty result means the assembly is collision-free.
[[nodiscard]] std::vector<Overlap> findOverlaps(const FurnitureComponent& root,
                                                Real tolerance = kEpsilon);

} // namespace rc::core
