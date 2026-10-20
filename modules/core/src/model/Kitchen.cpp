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
        cabinetSlot.plinthBeyondStart = i == 0 ? slot.plinthBeyondStart : 0.0;
        cabinetSlot.plinthBeyondEnd = i + 1 == cabinets.size() ? slot.plinthBeyondEnd : 0.0;
        cabinets[i]->layout(cabinetSlot);
        x += widths[i];
    }
}

// ---------------------------------------------------------------------------------------------
// KitchenCorner
// ---------------------------------------------------------------------------------------------

KitchenCorner::KitchenCorner(Side end, std::shared_ptr<Carcass> base, std::shared_ptr<Carcass> wall)
    : end_(end), base_(std::move(base)), wall_(std::move(wall))
{
    if (!base_ || !wall_) {
        throw std::invalid_argument("KitchenCorner: the cabinets must not be null");
    }
    adopt(base_);
    adopt(wall_);
}

Real KitchenCorner::baseWidth() const noexcept
{
    return base_->fixedWidth().value_or(kBaseWidth);
}

Real KitchenCorner::wallWidth() const noexcept
{
    return wall_->fixedWidth().value_or(kWallWidth);
}

void KitchenCorner::layoutChildren(const LayoutSlot& slot)
{
    if (slot.kitchen == nullptr) {
        throw std::logic_error("KitchenCorner must be laid out inside a kitchen run");
    }
    const KitchenFrame& k = *slot.kitchen;
    const Vec3& lo = box().min();
    const Vec3& hi = box().max();
    const bool right = end_ == Side::Right;
    // A column of the given width at the corner's end of the run.
    const auto column = [&](Real width, Real fromY, Real toY, Real depth) {
        const Real x0 = right ? hi.x - width : lo.x;
        return Aabb(Vec3(x0, fromY, lo.z), Vec3(x0 + width, toY, lo.z + depth));
    };

    baseFrame_ = {end_, k.cornerBase};
    LayoutSlot baseSlot = slot.withBox(column(baseWidth(), lo.y, k.baseTop, hi.z - lo.z));
    baseSlot.frontZ = baseSlot.box.max().z;
    baseSlot.corner = &baseFrame_;
    // The plinth runs on to the back of the side run's plinth, which meets its face.
    const ConstructionRules& r = slot.r();
    const Real stop = -((hi.z - lo.z) - r.kitchen.plinthSetback - r.carcassMaterial.thickness);
    (right ? baseSlot.plinthBeyondEnd : baseSlot.plinthBeyondStart) = stop;
    base_->layout(baseSlot);

    wallFrame_ = {end_, k.cornerWall};
    LayoutSlot wallSlot = slot.withBox(column(wallWidth(), k.wallBottom, hi.y, k.wallDepth));
    wallSlot.frontZ = wallSlot.box.max().z;
    wallSlot.corner = &wallFrame_;
    wall_->layout(wallSlot);
}

// ---------------------------------------------------------------------------------------------
// KitchenRun
// ---------------------------------------------------------------------------------------------

KitchenRun::KitchenRun(KitchenWall wall)
    : wall_(wall)
    , baseRow_(std::make_shared<CabinetRow>(RowLevel::Base))
    , worktop_(std::make_shared<Worktop>())
    , wallRow_(std::make_shared<CabinetRow>(RowLevel::Wall))
{
    adopt(baseRow_);
    adopt(worktop_);
    adopt(wallRow_);
}

KitchenCorner* KitchenRun::corner(Side end) const noexcept
{
    return corners_[end == Side::Left ? 0 : 1].get();
}

std::vector<Carcass*> KitchenRun::cabinets() const
{
    std::vector<Carcass*> all = baseRow_->cabinets();
    const std::vector<Carcass*> wall = wallRow_->cabinets();
    all.insert(all.end(), wall.begin(), wall.end());
    for (const Side end : {Side::Left, Side::Right}) {
        if (const KitchenCorner* c = corner(end)) {
            all.push_back(&c->baseCabinet());
            all.push_back(&c->wallCabinet());
        }
    }
    return all;
}

void KitchenRun::setLength(Real length)
{
    if (!(length > 0.0)) {
        throw std::invalid_argument("KitchenRun: the length must be > 0");
    }
    length_ = length;
    requestLayout();
}

void KitchenRun::layoutChildren(const LayoutSlot& slot)
{
    if (slot.kitchen == nullptr) {
        throw std::logic_error("KitchenRun must be laid out inside a kitchen");
    }
    const KitchenFrame& k = *slot.kitchen;
    const Vec3& lo = box().min();
    const Vec3& hi = box().max();

    // Where the rows may stand: the back run's corners take its ends; a side run's cabinets
    // stop in front of the back run's at the corner end (the left run's end, the right run's
    // start: both meet the back wall there).
    Real baseFrom = lo.x;
    Real baseTo = hi.x;
    Real wallFrom = lo.x;
    Real wallTo = hi.x;
    switch (wall_) {
    case KitchenWall::Back:
        for (const Side end : {Side::Left, Side::Right}) {
            KitchenCorner* c = corner(end);
            if (c == nullptr) {
                continue;
            }
            const Real width = std::max(c->baseWidth(), c->wallWidth());
            const Real x0 = end == Side::Left ? lo.x : hi.x - width;
            c->layout(slot.withBox(Aabb(Vec3(x0, lo.y, lo.z), Vec3(x0 + width, hi.y, hi.z))));
            if (end == Side::Left) {
                baseFrom += c->baseWidth();
                wallFrom += c->wallWidth();
            } else {
                baseTo -= c->baseWidth();
                wallTo -= c->wallWidth();
            }
        }
        break;
    case KitchenWall::Left:
        baseTo -= k.cornerBase;
        wallTo -= k.cornerWall;
        break;
    case KitchenWall::Right:
        baseFrom += k.cornerBase;
        wallFrom += k.cornerWall;
        break;
    }

    // 1. Base cabinets stand on the floor (each on its own plinth), as deep as the run. At a
    //    corner the side run's plinth reaches back to the face of the back run's.
    LayoutSlot baseSlot = slot.withBox(Aabb(Vec3(baseFrom, lo.y, lo.z), Vec3(baseTo, k.baseTop, hi.z)));
    const Real plinthReach = k.cornerBase - ((hi.z - lo.z) - slot.r().kitchen.plinthSetback);
    if (wall_ == KitchenWall::Left) {
        baseSlot.plinthBeyondEnd = plinthReach;
    } else if (wall_ == KitchenWall::Right) {
        baseSlot.plinthBeyondStart = plinthReach;
    }
    baseRow_->layout(baseSlot);

    // 2. Wall cabinets hang from the top line.
    wallRow_->layout(slot.withBox(Aabb(Vec3(wallFrom, k.wallBottom, lo.z),
                                       Vec3(wallTo, hi.y, lo.z + k.wallDepth))));

    // 3. The worktop lies on the base cabinets (the corners' too), as long as they are, with the
    //    openings their cabinets ask for: centred over the cabinet and in the worktop's depth.
    //    At a corner the back run's worktop runs on to the side wall, a side run's ends at the
    //    back run's front edge. Without base cabinets it has no length (the editor refuses
    //    that).
    frame_ = k;
    frame_.cutouts.clear();
    Real from = hi.x;
    Real to = lo.x;
    const std::vector<Carcass*> row = baseRow_->cabinets();
    std::vector<const Carcass*> base(row.begin(), row.end());
    for (const Side end : {Side::Left, Side::Right}) {
        if (const KitchenCorner* c = corner(end)) {
            base.push_back(&c->baseCabinet());
        }
    }
    for (const Carcass* cabinet : base) {
        from = std::min(from, cabinet->box().min().x);
        to = std::max(to, cabinet->box().max().x);
        if (cabinet->cutout()) {
            frame_.cutouts.push_back(planOf(*cabinet));
        }
    }
    if (to < from) {
        from = to = baseFrom;
    } else if (wall_ == KitchenWall::Left) {
        to = hi.x - k.worktopDepth;
    } else if (wall_ == KitchenWall::Right) {
        from = lo.x + k.worktopDepth;
    }
    frame_.startClosed = wall_ == KitchenWall::Right || corner(Side::Left) != nullptr;
    frame_.endClosed = wall_ == KitchenWall::Left || corner(Side::Right) != nullptr;
    LayoutSlot worktopSlot = slot.withBox(
        Aabb(Vec3(from, k.baseTop, lo.z), Vec3(std::max(from, to), k.baseTop, lo.z + k.worktopDepth)));
    worktopSlot.kitchen = &frame_;
    worktop_->layout(worktopSlot);
}

PanelCutout KitchenRun::planOf(const Carcass& cabinet) const
{
    const WorktopCutout& c = *cabinet.cutout();
    const Real worktopFront = box().min().z + frame_.worktopDepth;
    return {Vec2(cabinet.box().center().x, worktopFront - c.frontOffset - c.depth / 2.0), c.width,
            c.depth, c.radius};
}

std::vector<const Carcass*> KitchenRun::misplacedCutouts(const ConstructionRules& rules) const
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

// ---------------------------------------------------------------------------------------------
// Kitchen
// ---------------------------------------------------------------------------------------------

Kitchen::Kitchen() : backRun_(std::make_shared<KitchenRun>(KitchenWall::Back))
{
    adopt(backRun_);
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

KitchenRun* Kitchen::sideRun(Side side) const noexcept
{
    return sideRuns_[side == Side::Left ? 0 : 1].get();
}

std::vector<KitchenRun*> Kitchen::runs() const
{
    std::vector<KitchenRun*> all{backRun_.get()};
    for (const Side side : {Side::Left, Side::Right}) {
        if (KitchenRun* run = sideRun(side)) {
            all.push_back(run);
        }
    }
    return all;
}

void Kitchen::setSideRun(Side side, std::shared_ptr<KitchenRun> run, std::shared_ptr<KitchenCorner> corner)
{
    const KitchenWall wall = side == Side::Left ? KitchenWall::Left : KitchenWall::Right;
    if (!run != !corner || (run && (run->wall() != wall || corner->end() != side))) {
        throw std::invalid_argument("Kitchen: a side run needs its own wall and its corner");
    }
    std::shared_ptr<KitchenRun>& slot = sideRuns_[side == Side::Left ? 0 : 1];
    std::shared_ptr<KitchenCorner>& cornerSlot = backRun_->corners_[side == Side::Left ? 0 : 1];
    if (slot) {
        detachChildren([&](const FurnitureComponent& c) { return &c == slot.get(); });
        backRun_->detachChildren([&](const FurnitureComponent& c) { return &c == cornerSlot.get(); });
    }
    slot = std::move(run);
    cornerSlot = std::move(corner);
    if (slot) {
        // The tree reads back, left, right; the corner after the back run's rows.
        adoptAt(side == Side::Left ? 1 : children().size(), slot);
        backRun_->adoptAt(side == Side::Left ? 3 : backRun_->children().size(), cornerSlot);
    }
    requestLayout();
}

std::vector<const Carcass*> Kitchen::misplacedCutouts(const ConstructionRules& rules) const
{
    std::vector<const Carcass*> all;
    for (const KitchenRun* run : runs()) {
        const std::vector<const Carcass*> some = run->misplacedCutouts(rules);
        all.insert(all.end(), some.begin(), some.end());
    }
    return all;
}

void Kitchen::layoutChildren(const LayoutSlot& slot)
{
    const Vec3& lo = box().min();
    const Vec3& hi = box().max();
    const ConstructionRules& r = slot.r();
    const Real depth = hi.z - lo.z;
    const Real front = r.frontMaterial.thickness;
    frame_.baseTop = lo.y + r.kitchen.plinthHeight + sizes_.baseHeight;
    frame_.wallBottom = hi.y - sizes_.wallHeight;
    frame_.wallDepth = sizes_.wallDepth;
    frame_.worktopDepth = sizes_.worktopDepth;
    frame_.cornerBase = depth + front;
    frame_.cornerWall = sizes_.wallDepth + front;
    frame_.cutouts.clear();

    // The back run is the product box itself.
    LayoutSlot runSlot = slot.withBox(box());
    runSlot.kitchen = &frame_;
    backRun_->placement_ = {};
    backRun_->layout(runSlot);

    // A side run in its own coordinates (X from 0 to its length, Z from its wall), turned onto
    // its wall: the left run faces +X and ends at the back wall, the right run faces -X and
    // starts there.
    for (const Side side : {Side::Left, Side::Right}) {
        KitchenRun* run = sideRun(side);
        if (run == nullptr) {
            continue;
        }
        const Real length = run->length();
        run->placement_ = side == Side::Left ? Placement{1, Vec3(lo.x, 0.0, lo.z + length)}
                                             : Placement{3, Vec3(hi.x, 0.0, lo.z)};
        run->layout(runSlot.withBox(Aabb(Vec3(0.0, lo.y, 0.0), Vec3(length, hi.y, depth))));
    }
}

} // namespace rc::core
