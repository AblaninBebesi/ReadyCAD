#include "readycad/core/builders/WardrobeBuilder.hpp"

#include "readycad/core/model/Drawer.hpp"

#include <stdexcept>
#include <utility>
#include <vector>

namespace rc::core {

WardrobeBuilder& WardrobeBuilder::name(std::string name)
{
    name_ = std::move(name);
    return *this;
}

WardrobeBuilder& WardrobeBuilder::dimensions(const Size3D& dimensions)
{
    dimensions_ = dimensions;
    return *this;
}

WardrobeBuilder& WardrobeBuilder::rules(ConstructionRules rules)
{
    rules_ = std::move(rules);
    return *this;
}

WardrobeBuilder& WardrobeBuilder::joints(JointType top, JointType bottom)
{
    topJoint_ = top;
    bottomJoint_ = bottom;
    return *this;
}

WardrobeBuilder& WardrobeBuilder::backMount(BackMountType type)
{
    backMount_ = type;
    return *this;
}

WardrobeBuilder& WardrobeBuilder::sections(int count)
{
    sections_ = count;
    return *this;
}

WardrobeBuilder& WardrobeBuilder::sectionWidth(int index, Real width)
{
    sectionWidths_[index] = width;
    return *this;
}

WardrobeBuilder& WardrobeBuilder::shelvesPerSection(int count)
{
    shelves_ = count;
    return *this;
}

WardrobeBuilder& WardrobeBuilder::doorsPerSection(int count)
{
    doors_ = count;
    return *this;
}

WardrobeBuilder& WardrobeBuilder::drawers(int count, std::optional<Real> compartmentHeight)
{
    drawers_ = count;
    drawerCompartmentHeight_ = compartmentHeight;
    return *this;
}

std::shared_ptr<Product> WardrobeBuilder::build() const
{
    if (sections_ < 1) {
        throw std::invalid_argument("WardrobeBuilder: at least one section is required");
    }
    if (shelves_ < 0 || doors_ < 0 || drawers_ < 0) {
        throw std::invalid_argument("WardrobeBuilder: counts must not be negative");
    }
    for (const auto& [index, width] : sectionWidths_) {
        if (index < 0 || index >= sections_ || width <= 0.0) {
            throw std::invalid_argument("WardrobeBuilder: invalid section width");
        }
    }
    if (drawerCompartmentHeight_ && *drawerCompartmentHeight_ <= 0.0) {
        throw std::invalid_argument("WardrobeBuilder: drawer compartment height must be > 0");
    }

    auto product = std::make_shared<Product>(name_, rules_);
    auto carcass = std::make_shared<Carcass>();
    carcass->setTopJoint(topJoint_);
    carcass->setBottomJoint(bottomJoint_);
    carcass->setBackMount(backMount_);
    product->add(carcass);

    Section& interior = carcass->interior();
    std::vector<Section*> columns;
    if (sections_ == 1) {
        columns.push_back(&interior);
    } else {
        for (const auto& column : interior.splitHorizontally(sections_)) {
            columns.push_back(column.get());
        }
    }

    for (int i = 0; i < sections_; ++i) {
        Section& column = *columns[static_cast<std::size_t>(i)];
        if (const auto width = sectionWidths_.find(i); width != sectionWidths_.end()) {
            column.setFixedSize(width->second);
        }

        Section* upper = &column;
        if (drawers_ > 0) {
            if (!drawerCompartmentHeight_) {
                column.setDrawers(drawers_);
                continue; // drawers only
            }
            const auto rows = column.splitVertically(2);
            rows[0]->setFixedSize(*drawerCompartmentHeight_);
            rows[0]->setDrawers(drawers_);
            upper = rows[1].get();
        }
        upper->setAutoShelfCount(shelves_);
        upper->setDoors(doors_);
    }

    product->setDimensions(dimensions_); // first layout of the whole tree
    return product;
}

} // namespace rc::core
