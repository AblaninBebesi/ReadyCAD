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
        bounds.expand(c->box());
    }
    return bounds;
}

std::vector<Overlap> findOverlaps(const FurnitureComponent& root, Real tolerance)
{
    // O(n^2) is fine: a cabinet has tens of panels, a wardrobe about a hundred.
    const std::vector<const FurnitureComponent*> panels = collectPanels(root);
    std::vector<Overlap> overlaps;
    for (std::size_t i = 0; i < panels.size(); ++i) {
        for (std::size_t j = i + 1; j < panels.size(); ++j) {
            const FurnitureComponent& a = *panels[i];
            const FurnitureComponent& b = *panels[j];
            if (!a.box().intersects(b.box(), tolerance)) {
                continue;
            }
            if (a.allowsOverlapWith(b) || b.allowsOverlapWith(a)) {
                continue;
            }
            overlaps.push_back({&a, &b, a.box().intersection(b.box())});
        }
    }
    return overlaps;
}

} // namespace rc::core
