#include "readycad/core/model/Queries.hpp"

#include "readycad/core/Polygon.hpp"

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

namespace {

/// False if a shaped panel has no material in the region (product coordinates) where its box
/// meets another one's: the other part stands in its notch. Plain panels fill their box.
bool hasMaterialIn(const FurnitureComponent& part, const Aabb& region, Real tolerance)
{
    const Panel& panel = *part.panel();
    if (panel.outline().empty()) {
        return true;
    }
    // The region in the panel's own coordinates, then on its face: x along its length, y along
    // its width, both from its box minimum.
    const Aabb local = part.placement().inverse().box(region);
    const Aabb& box = part.box();
    const int l = index(panel.lengthAxis());
    const int w = index(panel.widthAxis());
    const Vec2 lo(local.min()[l] - box.min()[l], local.min()[w] - box.min()[w]);
    const Vec2 hi(local.max()[l] - box.min()[l], local.max()[w] - box.min()[w]);
    return areaInside(outlinePoints(panel.outline()), lo, hi) > tolerance * tolerance;
}

} // namespace

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
            const Aabb region = boxes[i].intersection(boxes[j]);
            if (!hasMaterialIn(a, region, tolerance) || !hasMaterialIn(b, region, tolerance)) {
                continue; // one stands in the other's notch
            }
            overlaps.push_back({&a, &b, region});
        }
    }
    return overlaps;
}

} // namespace rc::core
