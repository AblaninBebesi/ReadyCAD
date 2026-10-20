#include "readycad/core/model/Queries.hpp"

namespace rc::core {

std::vector<const FurnitureComponent*> collectPanels(const FurnitureComponent& root)
{
    std::vector<const FurnitureComponent*> panels;
    root.visit([&panels](const FurnitureComponent& c) {
        if (c.hasPanel()) {
            panels.push_back(&c);
        }
    });
    return panels;
}

Aabb panelBounds(const FurnitureComponent& root)
{
    Aabb bounds;
    for (const FurnitureComponent* c : collectPanels(root)) {
        bounds.expand(c->productBox());
    }
    return bounds;
}

std::vector<Overlap> findOverlaps(const FurnitureComponent& root, Real tolerance)
{
    // O(n^2) is fine: a cabinet has tens of panels, a wardrobe about a hundred, a U kitchen a
    // few hundred. In product coordinates: the cabinets of two walls meet in the corners.
    const std::vector<const FurnitureComponent*> panels = collectPanels(root);
    std::vector<Aabb> boxes;
    boxes.reserve(panels.size());
    for (const FurnitureComponent* c : panels) {
        boxes.push_back(c->productBox());
    }
    std::vector<Overlap> overlaps;
    for (std::size_t i = 0; i < panels.size(); ++i) {
        for (std::size_t j = i + 1; j < panels.size(); ++j) {
            if (!boxes[i].intersects(boxes[j], tolerance)) {
                continue;
            }
            const FurnitureComponent& a = *panels[i];
            const FurnitureComponent& b = *panels[j];
            if (a.allowsOverlapWith(b) || b.allowsOverlapWith(a)) {
                continue;
            }
            overlaps.push_back({&a, &b, boxes[i].intersection(boxes[j])});
        }
    }
    return overlaps;
}

} // namespace rc::core
