#include "readycad/core/builders/KitchenBuilder.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace rc::core {

RowLevel levelOf(CabinetModule module) noexcept
{
    switch (module) {
    case CabinetModule::WallDoors:
    case CabinetModule::WallOpen:
        return RowLevel::Wall;
    case CabinetModule::BaseDoors:
    case CabinetModule::BaseDrawers:
    case CabinetModule::Sink:
    case CabinetModule::Hob:
        break;
    }
    return RowLevel::Base;
}

std::shared_ptr<Carcass> makeCabinet(CabinetModule module, std::optional<Real> width,
                                     Real nominalWidth)
{
    auto cabinet = std::make_shared<Carcass>();
    cabinet->setFixedWidth(width);
    Section& interior = cabinet->interior();
    const int doors = nominalWidth > 500.0 ? 2 : 1;

    if (levelOf(module) == RowLevel::Base) {
        cabinet->setPlinth(true);
        cabinet->setBottomJoint(JointType::Overlay);
        const bool roomForAppliance = module == CabinetModule::Sink || module == CabinetModule::Hob;
        cabinet->setTopStyle(roomForAppliance ? CarcassTop::UprightRails : CarcassTop::FlatRails);
    } else {
        cabinet->setTopJoint(JointType::Inset);
        cabinet->setBottomJoint(JointType::Overlay);
    }

    switch (module) {
    case CabinetModule::BaseDoors:
    case CabinetModule::WallDoors:
        interior.setAutoShelfCount(1);
        interior.setDoors(doors);
        break;
    case CabinetModule::BaseDrawers:
        interior.setDrawers(3);
        break;
    case CabinetModule::Sink:
        interior.setDoors(doors); // no shelf: the siphon and the pipes need the room
        cabinet->setCutout(WorktopCutout::standard(CutoutKind::Sink));
        break;
    case CabinetModule::Hob:
        interior.setAutoShelfCount(1);
        interior.setDoors(doors);
        cabinet->setCutout(WorktopCutout::standard(CutoutKind::Hob));
        break;
    case CabinetModule::WallOpen:
        interior.setAutoShelfCount(1);
        break;
    }
    // Door handles where the hand finds them: at the top of a base cabinet, at the bottom of a
    // wall cabinet.
    for (Door* door : interior.doors()) {
        HandlePlacement handle = *door->handle();
        handle.anchor = levelOf(module) == RowLevel::Base ? HandleAnchor::Top : HandleAnchor::Bottom;
        door->setHandle(handle);
    }
    return cabinet;
}

std::shared_ptr<KitchenCorner> makeCorner(Side end, CornerType type, Real filler)
{
    if (type == CornerType::LShaped) {
        return std::make_shared<KitchenCorner>(end, std::make_shared<CornerCabinet>(RowLevel::Base, end),
                                               std::make_shared<CornerCabinet>(RowLevel::Wall, end));
    }
    // One door: what shows of a blind corner cabinet is about as wide as an ordinary one-door
    // cabinet. Hinged away from the corner, where its wall is.
    const HingeSide hinge = end == Side::Right ? HingeSide::Left : HingeSide::Right;
    auto base = makeCabinet(CabinetModule::BaseDoors, KitchenCorner::kBaseWidth, 400.0);
    auto wall = makeCabinet(CabinetModule::WallDoors, KitchenCorner::kWallWidth, 400.0);
    for (Carcass* cabinet : {base.get(), wall.get()}) {
        cabinet->setBlindCorner(filler);
        for (Door* door : cabinet->interior().doors()) {
            door->setHinge(hinge);
        }
    }
    return std::make_shared<KitchenCorner>(end, std::move(base), std::move(wall));
}

KitchenBuilder& KitchenBuilder::name(std::string name)
{
    name_ = std::move(name);
    return *this;
}

KitchenBuilder& KitchenBuilder::rules(ConstructionRules rules)
{
    rules_ = std::move(rules);
    return *this;
}

KitchenBuilder& KitchenBuilder::length(Real length)
{
    length_ = length;
    return *this;
}

KitchenBuilder& KitchenBuilder::height(Real height)
{
    height_ = height;
    return *this;
}

KitchenBuilder& KitchenBuilder::depth(Real depth)
{
    depth_ = depth;
    return *this;
}

KitchenBuilder& KitchenBuilder::sizes(const Kitchen::Sizes& sizes)
{
    sizes_ = sizes;
    return *this;
}

KitchenBuilder& KitchenBuilder::add(CabinetModule module, std::optional<Real> width, KitchenWall wall)
{
    modules_.push_back({module, width, wall});
    return *this;
}

KitchenBuilder& KitchenBuilder::sideWall(Side side, Real length, CornerType corner)
{
    sideLengths_[side == Side::Left ? 0 : 1] = length;
    corners_[side == Side::Left ? 0 : 1] = corner;
    return *this;
}

std::shared_ptr<Product> KitchenBuilder::build() const
{
    auto product = std::make_shared<Product>(name_, rules_);
    auto kitchen = std::make_shared<Kitchen>();
    kitchen->setSizes(sizes_);
    product->add(kitchen);
    for (const Side side : {Side::Left, Side::Right}) {
        if (const std::optional<Real>& length = sideLengths_[side == Side::Left ? 0 : 1]) {
            auto run = std::make_shared<KitchenRun>(side == Side::Left ? KitchenWall::Left
                                                                       : KitchenWall::Right);
            run->setLength(*length); // throws for <= 0
            kitchen->setSideRun(side, std::move(run), makeCorner(side, corners_[side == Side::Left ? 0 : 1]));
        }
    }
    for (const Module& m : modules_) {
        const bool hasRun = m.wall == KitchenWall::Back
                         || kitchen->sideRun(m.wall == KitchenWall::Left ? Side::Left : Side::Right);
        if (!hasRun) {
            throw std::invalid_argument("KitchenBuilder: a cabinet for a wall without a run");
        }
    }

    for (KitchenRun* run : kitchen->runs()) {
        // The length its rows have, about: what the corners leave. With it, what an automatic
        // cabinet of each row will be about: it decides one door or two.
        Real available[2] = {run->length(), run->length()};
        if (run->wall() == KitchenWall::Back) {
            available[0] = available[1] = length_;
            for (const Side end : {Side::Left, Side::Right}) {
                if (const KitchenCorner* c = run->corner(end)) {
                    available[0] -= c->baseWidth();
                    available[1] -= c->wallWidth();
                }
            }
        } else {
            const KitchenCorner& c = *kitchen->backRun().corner(run->wall() == KitchenWall::Left ? Side::Left
                                                                                                : Side::Right);
            const CornerCabinet* base = c.lCabinet(RowLevel::Base);
            available[0] -= base != nullptr ? base->legAlongSide() : depth_ + rules_.frontMaterial.thickness;
            available[1] -= base != nullptr ? c.lCabinet(RowLevel::Wall)->legAlongSide()
                                            : sizes_.wallDepth + rules_.frontMaterial.thickness;
        }
        Real fixed[2] = {0.0, 0.0};
        int automatic[2] = {0, 0};
        bool hasBase = false;
        for (const Module& m : modules_) {
            if (m.wall != run->wall()) {
                continue;
            }
            const auto row = static_cast<std::size_t>(levelOf(m.module));
            if (m.width) {
                if (!(*m.width > 0.0)) {
                    throw std::invalid_argument("KitchenBuilder: a cabinet width must be > 0");
                }
                fixed[row] += *m.width;
            } else {
                ++automatic[row];
            }
            hasBase = hasBase || levelOf(m.module) == RowLevel::Base;
        }
        if (!hasBase) {
            throw std::invalid_argument("KitchenBuilder: every kitchen run needs a base cabinet");
        }
        for (const Module& m : modules_) {
            if (m.wall != run->wall()) {
                continue;
            }
            const RowLevel level = levelOf(m.module);
            const auto row = static_cast<std::size_t>(level);
            const Real nominal =
                m.width ? *m.width : (available[row] - fixed[row]) / std::max(1, automatic[row]);
            CabinetRow& target = level == RowLevel::Base ? run->baseRow() : run->wallRow();
            target.add(makeCabinet(m.module, m.width, nominal));
        }
    }
    product->setDimensions({length_, height_, depth_}); // first layout of the whole tree
    return product;
}

} // namespace rc::core
