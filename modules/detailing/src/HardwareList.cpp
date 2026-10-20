#include "readycad/detailing/HardwareList.hpp"

#include "readycad/core/model/Drawer.hpp"
#include "readycad/core/model/Kitchen.hpp"
#include "readycad/core/model/Parts.hpp"
#include "readycad/core/model/Queries.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>

namespace rc::detailing {

using core::FastenerType;
using core::Real;

namespace {

std::string number(Real value)
{
    char buffer[32];
    if (std::abs(value - std::round(value)) < 1e-9) {
        std::snprintf(buffer, sizeof buffer, "%.0f", value);
    } else {
        std::snprintf(buffer, sizeof buffer, "%.1f", value);
    }
    return buffer;
}

} // namespace

std::vector<HardwareItem> hardwareList(const core::Product& product)
{
    const core::ConstructionRules& rules = product.rules();
    int confirmats = 0;
    int dowels = 0;
    int eccentrics = 0;
    int hingeCups = 0;
    int foldingCups = 0;
    for (const core::FurnitureComponent* part : core::collectPanels(product)) {
        const bool folding = dynamic_cast<const core::FoldingDoor*>(part) != nullptr;
        for (const core::Drilling& d : part->panel()->drillings()) {
            if (folding) {
                foldingCups += d.fastener == FastenerType::Hinge ? 1 : 0;
                continue;
            }
            const bool edge = !d.isFaceHole();
            confirmats += edge && d.fastener == FastenerType::Confirmat ? 1 : 0;
            dowels += edge && d.fastener == FastenerType::Dowel ? 1 : 0;
            eccentrics += edge && d.fastener == FastenerType::Eccentric ? 1 : 0;
            hingeCups += d.fastener == FastenerType::Hinge
                                 && std::abs(d.diameter - rules.hinge.cupDiameter) < 0.01
                             ? 1
                             : 0;
        }
    }

    int supports = 0;
    for (const core::Shelf* shelf : core::findAll<core::Shelf>(product)) {
        supports += shelf->mount() == core::ShelfMount::Supports ? 4 : 0;
    }

    std::vector<HardwareItem> list;
    const auto add = [&list](HardwareKind kind, std::string spec, int quantity,
                             std::string article = {}) {
        if (quantity > 0) {
            list.push_back({kind, std::move(spec), std::move(article), quantity});
        }
    };
    add(HardwareKind::Confirmat,
        number(rules.confirmat.faceDiameter) + " × " + number(rules.confirmat.length), confirmats);
    add(HardwareKind::Dowel, number(rules.dowel.diameter) + " × " + number(rules.dowel.length),
        dowels);
    add(HardwareKind::Eccentric, "Ø" + number(rules.eccentric.camDiameter), eccentrics);
    add(HardwareKind::ShelfSupport, "Ø" + number(rules.shelfSupport.diameter), supports);
    add(HardwareKind::Hinge, "Ø" + number(rules.hinge.cupDiameter), hingeCups);
    add(HardwareKind::FoldingHinge, "Ø" + number(rules.hinge.cupDiameter), foldingCups);

    // Handles per model, in catalogue order.
    std::map<std::string, int> handles;
    for (const core::FrontPanel* front : core::findAll<core::FrontPanel>(product)) {
        if (const core::HandleModel* model = front->handleModel(rules)) {
            ++handles[model->id];
        }
    }
    for (const core::HandleModel& model : rules.handles.catalog) {
        if (const auto it = handles.find(model.id); it != handles.end()) {
            add(HardwareKind::Handle, model.name, it->second, model.article);
        }
    }

    // Drawer slides: one pair per drawer, grouped by length.
    std::map<Real, int> slides;
    for (const core::Drawer* drawer : core::findAll<core::Drawer>(product)) {
        ++slides[drawer->slideLength()];
    }
    for (const auto& [length, pairs] : slides) {
        add(HardwareKind::DrawerSlide, number(length) + " mm", pairs);
    }

    // Kitchens: legs under every cabinet with a plinth (two more for each further metre of
    // width), a clip on every front leg for the plinth, two hangers per wall cabinet.
    int legs = 0;
    int hangers = 0;
    for (const core::Carcass* carcass : core::findAll<core::Carcass>(product)) {
        if (carcass->hasPlinth()) {
            const Real width = carcass->box().size().x;
            legs += 2 * (2 + static_cast<int>(std::max(0.0, width - 1.0) / 1000.0));
        }
        const auto* row = dynamic_cast<const core::CabinetRow*>(carcass->parent());
        const auto* corner = dynamic_cast<const core::KitchenCorner*>(carcass->parent());
        const bool wall = (row != nullptr && row->level() == core::RowLevel::Wall)
                       || (corner != nullptr && corner->blindCabinet(core::RowLevel::Wall) == carcass);
        if (wall) {
            hangers += 2;
        }
    }
    // An L corner cabinet: six legs under its two legs (both ends, the inner corner, the back
    // corner), two hangers above.
    for (const core::CornerCabinet* corner : core::findAll<core::CornerCabinet>(product)) {
        if (corner->plinth() != nullptr) {
            legs += 6;
        } else {
            hangers += 2;
        }
    }
    add(HardwareKind::Leg, number(rules.kitchen.plinthHeight) + " mm", legs);
    add(HardwareKind::PlinthClip, "", legs / 2);
    add(HardwareKind::Hanger, "", hangers);
    return list;
}

} // namespace rc::detailing
