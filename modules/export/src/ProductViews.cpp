#include "readycad/export/ProductViews.hpp"

#include "readycad/core/model/Carcass.hpp"
#include "readycad/core/model/Drawer.hpp"
#include "readycad/core/model/Kitchen.hpp"
#include "readycad/core/model/Parts.hpp"
#include "readycad/core/model/Queries.hpp"
#include "readycad/core/model/Section.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <utility>

namespace rc::exporting {

using core::Aabb;
using core::ComponentKind;
using core::FurnitureComponent;

namespace {

constexpr Real kEps = 1e-6;

struct Rect {
    Vec2 min{0.0};
    Vec2 max{0.0};
};

/// A panel as seen in a view. "Nearness" grows towards the viewer.
struct Item {
    const FurnitureComponent* component = nullptr;
    Rect rect;
    Real nearMin = 0.0;
    Real nearMax = 0.0;
    bool cut = false; ///< section: the plane goes through it
};

bool isFront(ComponentKind kind) noexcept
{
    return kind == ComponentKind::Door || kind == ComponentKind::DrawerFront
        || kind == ComponentKind::BlindPanel;
}

bool isWall(ComponentKind kind) noexcept
{
    return kind == ComponentKind::SideWall || kind == ComponentKind::Partition;
}

Rect projectFront(const Aabb& box)
{
    return {{box.min().x, box.min().y}, {box.max().x, box.max().y}};
}

/// Section seen from the left: depth to the right (back on the left), height up.
Rect projectSide(const Aabb& box)
{
    return {{box.min().z, box.min().y}, {box.max().z, box.max().y}};
}

/// Plan: X to the right, the back wall at the top (Z = 0), the room below it.
Rect projectPlan(const Aabb& box)
{
    return {{box.min().x, -box.max().z}, {box.max().x, -box.min().z}};
}

/// A part of a wall cabinet of a kitchen (of a row or of a corner): above the plan's cut.
bool inWallCabinet(const FurnitureComponent& part)
{
    const FurnitureComponent* child = &part;
    for (const FurnitureComponent* p = part.parent(); p != nullptr; child = p, p = p->parent()) {
        if (p->kind() == ComponentKind::CabinetRow
            && static_cast<const core::CabinetRow*>(p)->level() == core::RowLevel::Wall) {
            return true;
        }
        if (p->kind() == ComponentKind::KitchenCorner
            && &static_cast<const core::KitchenCorner*>(p)->wallCabinet() == child) {
            return true;
        }
    }
    return false;
}

const core::Product* productOf(const FurnitureComponent& root)
{
    return dynamic_cast<const core::Product*>(&root.root());
}

std::vector<Item> collectItems(const FurnitureComponent& root, ProductView view)
{
    std::vector<Item> items;
    if (view == ProductView::Plan) {
        for (const FurnitureComponent* c : core::collectPanels(root.root())) {
            if (inWallCabinet(*c)) {
                continue;
            }
            const Aabb box = c->productBox();
            Item item;
            item.component = c;
            item.rect = projectPlan(box);
            item.nearMin = box.min().y;
            item.nearMax = box.max().y;
            items.push_back(item);
        }
        return items;
    }
    const Real xc = view == ProductView::Section ? sectionPlaneX(root) : 0.0;
    for (const FurnitureComponent* c : core::collectPanels(root)) {
        const ComponentKind kind = c->kind();
        const Aabb& box = c->box();
        Item item;
        item.component = c;
        if (view == ProductView::Section) {
            if (box.max().x <= xc + kEps) {
                continue; // in front of the plane: removed by the cut
            }
            item.cut = box.min().x < xc - kEps;
            item.rect = projectSide(box);
            item.nearMin = -box.max().x;
            item.nearMax = item.cut ? -xc : -box.min().x;
        } else {
            if (kind == ComponentKind::BackPanel) {
                continue; // behind everything, its edges hide in the grooves
            }
            if (view == ProductView::FrontOpen && isFront(kind)) {
                continue;
            }
            item.rect = projectFront(box);
            item.nearMin = box.min().z;
            item.nearMax = box.max().z;
        }
        items.push_back(item);
    }

    if (view == ProductView::Section) {
        // Beyond the plane we see up to the next wall; what is behind that wall is hidden.
        Real wallX = std::numeric_limits<Real>::max();
        for (const Item& item : items) {
            if (!item.cut && isWall(item.component->kind())) {
                wallX = std::min(wallX, item.component->box().min().x);
            }
        }
        std::erase_if(items, [&](const Item& item) {
            return !item.cut && item.component->box().min().x > wallX + kEps;
        });
    }
    return items;
}

// --- Hidden line removal --------------------------------------------------------------------

using Interval = std::pair<Real, Real>;

void subtract(std::vector<Interval>& intervals, Real from, Real to)
{
    std::vector<Interval> result;
    for (const auto& [a, b] : intervals) {
        if (to <= a + kEps || from >= b - kEps) {
            result.emplace_back(a, b);
            continue;
        }
        if (from > a + kEps) {
            result.emplace_back(a, from);
        }
        if (to < b - kEps) {
            result.emplace_back(to, b);
        }
    }
    intervals = std::move(result);
}

/// Visible pieces of the edges of axis-aligned rectangles, merged where they overlap.
class EdgeCollector {
public:
    void addEdges(const Item& item, const std::vector<const Item*>& occluders)
    {
        const Rect& r = item.rect;
        horizontal(r.min.y, r.min.x, r.max.x, occluders);
        horizontal(r.max.y, r.min.x, r.max.x, occluders);
        vertical(r.min.x, r.min.y, r.max.y, occluders);
        vertical(r.max.x, r.min.y, r.max.y, occluders);
    }

    void emit(Drawing& drawing, std::size_t layer)
    {
        for (auto& [y, intervals] : horizontal_) {
            for (const auto& [a, b] : merged(intervals)) {
                drawing.line(layer, Vec2(a, y), Vec2(b, y));
            }
        }
        for (auto& [x, intervals] : vertical_) {
            for (const auto& [a, b] : merged(intervals)) {
                drawing.line(layer, Vec2(x, a), Vec2(x, b));
            }
        }
    }

private:
    static Real key(Real v) { return std::round(v * 1e4) / 1e4; }

    static std::vector<Interval> merged(std::vector<Interval> intervals)
    {
        std::sort(intervals.begin(), intervals.end());
        std::vector<Interval> out;
        for (const Interval& i : intervals) {
            if (!out.empty() && i.first <= out.back().second + kEps) {
                out.back().second = std::max(out.back().second, i.second);
            } else {
                out.push_back(i);
            }
        }
        return out;
    }

    void horizontal(Real y, Real x0, Real x1, const std::vector<const Item*>& occluders)
    {
        std::vector<Interval> visible{{x0, x1}};
        for (const Item* o : occluders) {
            if (o->rect.min.y + kEps < y && y < o->rect.max.y - kEps) {
                subtract(visible, o->rect.min.x, o->rect.max.x);
            }
        }
        auto& list = horizontal_[key(y)];
        list.insert(list.end(), visible.begin(), visible.end());
    }

    void vertical(Real x, Real y0, Real y1, const std::vector<const Item*>& occluders)
    {
        std::vector<Interval> visible{{y0, y1}};
        for (const Item* o : occluders) {
            if (o->rect.min.x + kEps < x && x < o->rect.max.x - kEps) {
                subtract(visible, o->rect.min.y, o->rect.max.y);
            }
        }
        auto& list = vertical_[key(x)];
        list.insert(list.end(), visible.begin(), visible.end());
    }

    std::map<Real, std::vector<Interval>> horizontal_;
    std::map<Real, std::vector<Interval>> vertical_;
};

// --- Layers -----------------------------------------------------------------------------------

Layer visibleLayer()
{
    return {std::string(viewLayers::kVisible), 7, {0, 0, 0}, LineType::Continuous,
            LineWeight::Normal};
}

Layer textLayer()
{
    return {std::string(viewLayers::kText), 7, {0, 0, 0}, LineType::Continuous, LineWeight::Thin};
}

Layer dimensionLayer()
{
    return {std::string(viewLayers::kDimensions), 2, {40, 40, 40}, LineType::Continuous,
            LineWeight::Thin};
}

// --- Annotations ------------------------------------------------------------------------------

void handles(Drawing& drawing, const FurnitureComponent& root)
{
    const core::Product* product = productOf(root);
    if (product == nullptr) {
        return;
    }
    const std::size_t layer = drawing.addLayer({std::string(viewLayers::kHandles), 8,
                                                {60, 60, 60}, LineType::Continuous,
                                                LineWeight::Thin});
    for (const core::FrontPanel* front : core::findAll<core::FrontPanel>(root)) {
        const auto geometry = front->handleGeometry(product->rules());
        if (!geometry) {
            continue;
        }
        const core::HandleModel& model = *geometry->model;
        const Vec2 c(geometry->center.x, geometry->center.y);
        if (model.type == core::HandleType::Knob) {
            drawing.circle(layer, c, model.length / 2.0);
            continue;
        }
        const bool vertical = geometry->along == core::Axis::Y;
        const Vec2 half = vertical ? Vec2(model.width, model.length) * 0.5
                                   : Vec2(model.length, model.width) * 0.5;
        drawing.rectangle(layer, c - half, c + half);
    }
}

void sectionTrace(Drawing& drawing, const Bounds& bounds, Real xc, Real h)
{
    const std::size_t line = drawing.addLayer({std::string(viewLayers::kCutLine), 1, {0, 0, 0},
                                               LineType::Center, LineWeight::Thin});
    const std::size_t thick = drawing.addLayer({std::string(viewLayers::kSection), 7, {0, 0, 0},
                                                LineType::Continuous, LineWeight::Thick});
    const std::size_t text = drawing.addLayer(textLayer());
    const Real bottom = bounds.min.y - 1.5 * h;
    const Real top = bounds.max.y + 1.5 * h;
    drawing.line(line, Vec2(xc, bottom), Vec2(xc, top));
    for (const Real y : {bottom, top}) {
        const Real inward = y < bounds.min.y ? 1.0 : -1.0;
        drawing.line(thick, Vec2(xc, y), Vec2(xc, y + inward * 1.2 * h));
        // Arrow: the section is looked at from the left, towards +X.
        drawing.line(thick, Vec2(xc, y), Vec2(xc + 1.6 * h, y));
        drawing.add(thick, Polyline{{Vec2(xc + 2.4 * h, y), Vec2(xc + 1.4 * h, y + 0.3 * h),
                                     Vec2(xc + 1.4 * h, y - 0.3 * h)},
                                    true, true});
        drawing.text(text, {Vec2(xc + 2.8 * h, y), "A", 1.2 * h, 0.0, HAlign::Left,
                            VAlign::Middle, true});
    }
}

void frontDimensions(Drawing& drawing, const FurnitureComponent& product, const Bounds& b, Real h,
                     bool interior)
{
    const std::size_t dims = drawing.addLayer(dimensionLayer());
    drawing.dimension(dims, {b.min, Vec2(b.max.x, b.min.y), -3.0 * h, "", h, h});
    drawing.dimension(dims, {b.min, Vec2(b.min.x, b.max.y), 3.0 * h, "", h, h});
    if (!interior) {
        return;
    }
    // Compartments = leaves of the section tree.
    std::vector<const core::Section*> leaves;
    Real interiorTop = -std::numeric_limits<Real>::max();
    for (const core::Section* section : core::findAll<core::Section>(product)) {
        if (section->subsections().empty()) {
            leaves.push_back(section);
            interiorTop = std::max(interiorTop, section->box().max().y);
        }
    }
    // Inside dimensions are only drawn where their value fits between the arrows: at small
    // scales they would be a mess.
    constexpr Real kFits = 5.5; // in text heights
    std::vector<std::pair<Real, Real>> topWidths;
    for (const core::Section* section : leaves) {
        const Aabb& box = section->box();
        const Vec2 lo(box.min().x, box.min().y);
        const Vec2 hi(box.max().x, box.max().y);
        // Clear widths of the upper compartments as a chain above the view; widths of lower
        // compartments (rows of a split) inside them.
        if (std::abs(hi.y - interiorTop) < kEps) {
            const bool known = std::any_of(topWidths.begin(), topWidths.end(), [&](const auto& w) {
                return std::abs(w.first - lo.x) < kEps && std::abs(w.second - hi.x) < kEps;
            });
            if (!known) {
                topWidths.emplace_back(lo.x, hi.x);
                drawing.dimension(dims, {Vec2(lo.x, b.max.y), Vec2(hi.x, b.max.y), 4.0 * h, "",
                                         h, h});
            }
        } else if (section->drawerStack() == nullptr && hi.x - lo.x > kFits * h) {
            drawing.dimension(dims, {Vec2(lo.x, hi.y), hi, -2.2 * h, "", h, h});
        }
        if (section->drawerStack() != nullptr) {
            continue; // drawer boxes fill it: nothing to measure inside
        }
        // Clear heights between the shelves: what the assembler checks with a tape measure.
        std::vector<Real> levels{lo.y};
        std::vector<core::Shelf*> shelves = section->shelves();
        std::sort(shelves.begin(), shelves.end(), [](const core::Shelf* a, const core::Shelf* s) {
            return a->box().min().y < s->box().min().y;
        });
        for (const core::Shelf* shelf : shelves) {
            levels.push_back(shelf->box().min().y);
            levels.push_back(shelf->box().max().y);
        }
        levels.push_back(hi.y);
        for (std::size_t i = 0; i + 1 < levels.size(); i += 2) {
            if (levels[i + 1] - levels[i] > kFits * h) {
                drawing.dimension(dims, {Vec2(lo.x, levels[i]), Vec2(lo.x, levels[i + 1]),
                                         -2.5 * h, "", h, h});
            }
        }
    }
}

void sectionDimensions(Drawing& drawing, const FurnitureComponent& root,
                       const std::vector<Item>& items, const Bounds& b, Real h)
{
    const std::size_t dims = drawing.addLayer(dimensionLayer());
    drawing.dimension(dims, {b.min, Vec2(b.max.x, b.min.y), -3.0 * h, "", h, h});
    drawing.dimension(dims, {b.min, Vec2(b.min.x, b.max.y), 3.0 * h, "", h, h});
    const auto* product = dynamic_cast<const core::Product*>(&root);
    if (const core::Carcass* carcass = product != nullptr ? product->carcass() : nullptr) {
        const Rect side = projectSide(carcass->leftSide().box());
        drawing.dimension(dims, {Vec2(side.min.x, b.max.y), Vec2(side.max.x, b.max.y), 2.0 * h,
                                 "", h, h});
    }
    // Depth of the lowest cut shelf.
    const Item* shelf = nullptr;
    for (const Item& item : items) {
        if (item.cut && item.component->kind() == ComponentKind::Shelf
            && (shelf == nullptr || item.rect.min.y < shelf->rect.min.y)) {
            shelf = &item;
        }
    }
    if (shelf != nullptr) {
        drawing.dimension(dims, {Vec2(shelf->rect.min.x, shelf->rect.max.y), shelf->rect.max,
                                 1.5 * h, "", h, h});
    }
}

/// The plan's openings in the worktops (a sink, a hob), seen through from above.
void worktopCutouts(Drawing& drawing, const FurnitureComponent& root)
{
    const std::size_t layer = drawing.addLayer(visibleLayer());
    for (const core::Worktop* worktop : core::findAll<core::Worktop>(root.root())) {
        // Panel x along X, panel y along Z, both from the box minimum (Worktop::automaticOpenings).
        const core::Placement place = worktop->placement();
        const Aabb& box = worktop->box();
        for (const core::PanelCutout& opening : worktop->panel()->cutouts()) {
            Polyline outline;
            outline.closed = true;
            for (const Vec2& p : core::openingOutline(opening)) {
                const core::Vec3 at = place.point(core::Vec3(box.min().x + p.x, box.max().y, box.min().z + p.y));
                outline.points.emplace_back(at.x, -at.z);
            }
            drawing.add(layer, outline);
        }
    }
}

/// The plan's dimensions: the back wall along the top, a side run along its wall.
void planDimensions(Drawing& drawing, const FurnitureComponent& root, const Bounds& b, Real h)
{
    const std::size_t dims = drawing.addLayer(dimensionLayer());
    drawing.dimension(dims, {Vec2(b.min.x, b.max.y), b.max, 3.0 * h, "", h, h});
    const core::Product* product = productOf(root);
    const core::Kitchen* kitchen = product != nullptr ? product->kitchen() : nullptr;
    if (kitchen == nullptr) {
        drawing.dimension(dims, {b.min, Vec2(b.min.x, b.max.y), 3.0 * h, "", h, h});
        return;
    }
    for (const core::Side side : {core::Side::Left, core::Side::Right}) {
        if (const core::KitchenRun* run = kitchen->sideRun(side)) {
            const Rect r = projectPlan(run->productBox());
            // Walking up the left wall / down the right wall: the dimension stays outside.
            if (side == core::Side::Left) {
                drawing.dimension(dims, {Vec2(r.min.x, r.min.y), Vec2(r.min.x, r.max.y), 3.0 * h, "", h, h});
            } else {
                drawing.dimension(dims, {Vec2(r.max.x, r.max.y), Vec2(r.max.x, r.min.y), 3.0 * h, "", h, h});
            }
        }
    }
}

Bounds boundsOf(const std::vector<Item>& items)
{
    Bounds b;
    for (const Item& item : items) {
        b.add(item.rect.min);
        b.add(item.rect.max);
    }
    return b;
}

} // namespace

Real sectionPlaneX(const FurnitureComponent& root)
{
    const core::Section* first = nullptr;
    for (const core::Section* section : core::findAll<core::Section>(root)) {
        if (section->subsections().empty()
            && (first == nullptr || section->box().min().x < first->box().min().x)) {
            first = section;
        }
    }
    Aabb box;
    if (first != nullptr) {
        box = first->box();
    } else {
        for (const FurnitureComponent* c : core::collectPanels(root)) {
            box.expand(c->box()); // the root's own coordinates
        }
    }
    return box.min().x + 0.4 * box.size().x;
}

Bounds viewExtent(const FurnitureComponent& root, ProductView view)
{
    return boundsOf(collectItems(root, view));
}

Drawing drawProductView(const FurnitureComponent& product, ProductView view,
                        const ProductViewOptions& options)
{
    Drawing drawing;
    const std::vector<Item> items = collectItems(product, view);
    const Bounds bounds = boundsOf(items);
    const Real h = options.textHeight;

    if (view == ProductView::Section) {
        const std::size_t fill = drawing.addLayer({std::string(viewLayers::kSectionFill), 9,
                                                   {215, 215, 215}, LineType::Continuous,
                                                   LineWeight::Thin});
        for (const Item& item : items) {
            if (item.cut) {
                drawing.rectangle(fill, item.rect.min, item.rect.max, true);
            }
        }
    }

    // Visible edges of everything that is not cut.
    EdgeCollector visible;
    for (const Item& item : items) {
        if (item.cut) {
            continue;
        }
        std::vector<const Item*> occluders;
        for (const Item& other : items) {
            if (&other != &item && other.nearMin >= item.nearMax - kEps) {
                occluders.push_back(&other);
            }
        }
        visible.addEdges(item, occluders);
    }
    visible.emit(drawing, drawing.addLayer(visibleLayer()));

    if (view == ProductView::Section) {
        EdgeCollector cut;
        for (const Item& item : items) {
            if (item.cut) {
                cut.addEdges(item, {});
            }
        }
        cut.emit(drawing, drawing.addLayer({std::string(viewLayers::kSection), 7, {0, 0, 0},
                                            LineType::Continuous, LineWeight::Thick}));
    }

    if (view == ProductView::Front) {
        handles(drawing, product);
    }
    if (view == ProductView::FrontOpen) {
        sectionTrace(drawing, bounds, sectionPlaneX(product), h);
    }
    if (view == ProductView::Plan) {
        worktopCutouts(drawing, product);
    }
    if (options.dimensions && !bounds.empty) {
        if (view == ProductView::Section) {
            sectionDimensions(drawing, product, items, bounds, h);
        } else if (view == ProductView::Plan) {
            planDimensions(drawing, product, bounds, h);
        } else {
            frontDimensions(drawing, product, bounds, h, view == ProductView::FrontOpen);
        }
    }
    return drawing;
}

} // namespace rc::exporting
