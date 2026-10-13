#include "readycad/core/model/Kitchen.hpp"

#include <algorithm>
#include <stdexcept>

namespace rc::core {

// ---------------------------------------------------------------------------------------------
// CabinetRow
// ---------------------------------------------------------------------------------------------

CabinetRow::CabinetRow(RowLevel level) : level_(level) {}

std::vector<Carcass*> CabinetRow::cabinets() const
{
    std::vector<Carcass*> result;
    for (const Ptr& child : children()) {
        result.push_back(static_cast<Carcass*>(child.get()));
    }
    return result;
}

void CabinetRow::moveCabinet(ComponentId cabinet, int steps)
{
    const auto& all = children();
    const auto it = std::find_if(all.begin(), all.end(),
                                 [cabinet](const Ptr& c) { return c->id() == cabinet; });
    if (it == all.end()) {
        throw std::invalid_argument("CabinetRow: not a cabinet of this row");
    }
    const auto from = static_cast<long long>(it - all.begin());
    const long long to =
        std::clamp(from + steps, 0LL, static_cast<long long>(all.size()) - 1);
    moveChild(static_cast<std::size_t>(from), static_cast<std::size_t>(to));
    requestLayout();
}

bool CabinetRow::canAccept(const FurnitureComponent& child) const
{
    return child.kind() == ComponentKind::Carcass;
}

void CabinetRow::layoutChildren(const LayoutSlot& slot)
{
    const std::vector<Carcass*> cabinets = this->cabinets();
    std::vector<std::optional<Real>> fixed;
    for (const Carcass* cabinet : cabinets) {
        fixed.push_back(cabinet->fixedWidth());
    }
    const std::vector<Real> widths = distributeSizes(box().size().x, fixed);

    Real x = box().min().x;
    for (std::size_t i = 0; i < cabinets.size(); ++i) {
        const Aabb column(Vec3(x, box().min().y, box().min().z),
                          Vec3(x + widths[i], box().max().y, box().max().z));
        LayoutSlot cabinetSlot = slot.withBox(column);
        cabinetSlot.frontZ = column.max().z;
        cabinets[i]->layout(cabinetSlot);
        x += widths[i];
    }
}

// ---------------------------------------------------------------------------------------------
// Kitchen
// ---------------------------------------------------------------------------------------------

Kitchen::Kitchen()
    : baseRow_(std::make_shared<CabinetRow>(RowLevel::Base))
    , worktop_(std::make_shared<Worktop>())
    , wallRow_(std::make_shared<CabinetRow>(RowLevel::Wall))
{
    adopt(baseRow_);
    adopt(worktop_);
    adopt(wallRow_);
}

void Kitchen::setSizes(const Sizes& sizes)
{
    const bool positive = sizes.baseHeight > 0.0 && sizes.wallHeight > 0.0
                       && sizes.wallDepth > 0.0 && sizes.worktopDepth > 0.0;
    if (!positive) {
        throw std::invalid_argument("Kitchen: sizes must be positive");
    }
    sizes_ = sizes;
    requestLayout();
}

void Kitchen::layoutChildren(const LayoutSlot& slot)
{
    const Vec3& lo = box().min();
    const Vec3& hi = box().max();
    const ConstructionRules& r = slot.r();
    const Real baseTop = lo.y + r.kitchen.plinthHeight + sizes_.baseHeight;

    // 1. Base cabinets stand on the floor (each on its own plinth), as deep as the run.
    baseRow_->layout(slot.withBox(Aabb(lo, Vec3(hi.x, baseTop, hi.z))));

    // 2. Wall cabinets hang from the top line.
    wallRow_->layout(slot.withBox(Aabb(Vec3(lo.x, hi.y - sizes_.wallHeight, lo.z),
                                       Vec3(hi.x, hi.y, lo.z + sizes_.wallDepth))));

    // 3. The worktop lies on the base cabinets, as long as they are, with the openings their
    //    cabinets ask for: centred over the cabinet and in the worktop's depth. Without base
    //    cabinets it has no length (and the editor refuses that).
    Real from = hi.x;
    Real to = lo.x;
    frame_.cutouts.clear();
    for (const Carcass* cabinet : baseRow_->cabinets()) {
        from = std::min(from, cabinet->box().min().x);
        to = std::max(to, cabinet->box().max().x);
        if (cabinet->cutout()) {
            frame_.cutouts.push_back(planOf(*cabinet));
        }
    }
    if (to < from) {
        from = to = lo.x;
    }
    LayoutSlot worktopSlot = slot.withBox(
        Aabb(Vec3(from, baseTop, lo.z), Vec3(to, baseTop, lo.z + sizes_.worktopDepth)));
    worktopSlot.kitchen = &frame_;
    worktop_->layout(worktopSlot);
}

PanelCutout Kitchen::planOf(const Carcass& cabinet) const
{
    const WorktopCutout& c = *cabinet.cutout();
    const Real worktopFront = box().min().z + sizes_.worktopDepth;
    return {Vec2(cabinet.box().center().x, worktopFront - c.frontOffset - c.depth / 2.0), c.width,
            c.depth, c.radius};
}

std::vector<const Carcass*> Kitchen::misplacedCutouts(const ConstructionRules& rules) const
{
    // Plan rectangles: x along X, y along Z.
    struct Rect {
        Vec2 lo;
        Vec2 hi;
        [[nodiscard]] bool overlaps(const Rect& o, Real gap) const noexcept
        {
            return lo.x < o.hi.x + gap && o.lo.x < hi.x + gap && lo.y < o.hi.y + gap
                && o.lo.y < hi.y + gap;
        }
    };
    auto planOfBox = [](const Aabb& b) { return Rect{Vec2(b.min().x, b.min().z), Vec2(b.max().x, b.max().z)}; };
    const Real margin = rules.kitchen.cutoutMargin;
    const Rect worktop = planOfBox(worktop_->box());

    std::vector<const Carcass*> cabinets;
    std::vector<Rect> cutouts;
    for (const Carcass* cabinet : baseRow_->cabinets()) {
        if (cabinet->cutout()) {
            const PanelCutout c = planOf(*cabinet);
            const Vec2 half(c.length / 2.0, c.width / 2.0);
            cabinets.push_back(cabinet);
            cutouts.push_back({c.center - half, c.center + half});
        }
    }

    std::vector<const Carcass*> misplaced;
    for (std::size_t i = 0; i < cutouts.size(); ++i) {
        const Rect& r = cutouts[i];
        const Carcass& cabinet = *cabinets[i];
        bool ok = r.lo.x >= worktop.lo.x + margin - kEpsilon
               && r.hi.x <= worktop.hi.x - margin + kEpsilon
               && r.lo.y >= worktop.lo.y + margin - kEpsilon
               && r.hi.y <= worktop.hi.y - margin + kEpsilon;
        for (std::size_t j = 0; j < cutouts.size() && ok; ++j) {
            ok = j == i || !r.overlaps(cutouts[j], margin - kEpsilon);
        }
        // The appliance hangs into its own cabinet: between the walls, in front of the back...
        ok = ok && r.lo.x >= cabinet.leftSide().box().max().x - kEpsilon
          && r.hi.x <= cabinet.rightSide().box().min().x + kEpsilon
          && r.lo.y >= cabinet.back().box().max().z - kEpsilon
          && r.hi.y <= cabinet.box().max().z + kEpsilon;
        // ...and clear of the parts right under the worktop: the top panel or the rails.
        for (const PanelComponent* part : cabinet.frameParts()) {
            const bool underTheTop =
                part->kind() == ComponentKind::TopPanel || part->kind() == ComponentKind::Rail;
            ok = ok && !(underTheTop && r.overlaps(planOfBox(part->box()), -kEpsilon));
        }
        if (!ok) {
            misplaced.push_back(&cabinet);
        }
    }
    return misplaced;
}

} // namespace rc::core
