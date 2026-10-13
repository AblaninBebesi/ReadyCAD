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

KitchenBuilder& KitchenBuilder::add(CabinetModule module, std::optional<Real> width)
{
    modules_.push_back({module, width});
    return *this;
}

std::shared_ptr<Product> KitchenBuilder::build() const
{
    // What an automatic cabinet of each row will be about: it decides one door or two.
    Real fixed[2] = {0.0, 0.0};
    int automatic[2] = {0, 0};
    bool hasBase = false;
    for (const Module& m : modules_) {
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
        throw std::invalid_argument("KitchenBuilder: a kitchen needs a base cabinet");
    }

    auto product = std::make_shared<Product>(name_, rules_);
    auto kitchen = std::make_shared<Kitchen>();
    kitchen->setSizes(sizes_);
    product->add(kitchen);
    for (const Module& m : modules_) {
        const RowLevel level = levelOf(m.module);
        const auto row = static_cast<std::size_t>(level);
        const Real nominal =
            m.width ? *m.width : (length_ - fixed[row]) / std::max(1, automatic[row]);
        CabinetRow& target = level == RowLevel::Base ? kitchen->baseRow() : kitchen->wallRow();
        target.add(makeCabinet(m.module, m.width, nominal));
    }
    product->setDimensions({length_, height_, depth_}); // first layout of the whole tree
    return product;
}

} // namespace rc::core
