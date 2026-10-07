// Stage 6: hardware list (фурнитура) for the documents.

#include "readycad/core/builders/Catalog.hpp"
#include "readycad/core/builders/WardrobeBuilder.hpp"
#include "readycad/detailing/Detailing.hpp"
#include "readycad/detailing/HardwareList.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>

using namespace rc::core;
using namespace rc::detailing;

namespace {

int quantityOf(const std::vector<HardwareItem>& list, HardwareKind kind)
{
    int total = 0;
    for (const HardwareItem& item : list) {
        total += item.kind == kind ? item.quantity : 0;
    }
    return total;
}

} // namespace

TEST_CASE("Hardware of the reference wardrobe", "[detailing][hardware]")
{
    // 800 x 2000 x 560, 4 shelves on supports, 2 doors, top and bottom with confirmats.
    const auto wardrobe = WardrobeBuilder().shelvesPerSection(4).doorsPerSection(2).build();
    applyDetailing(*wardrobe);
    const auto list = hardwareList(*wardrobe);

    CHECK(quantityOf(list, HardwareKind::Confirmat) == 4 * 3);    // 4 joints, 3 screws each
    CHECK(quantityOf(list, HardwareKind::ShelfSupport) == 4 * 4); // 4 pins per shelf
    CHECK(quantityOf(list, HardwareKind::Hinge) == 2 * 4);        // doors over 1600: 4 hinges
    CHECK(quantityOf(list, HardwareKind::Handle) == 2);
    CHECK(quantityOf(list, HardwareKind::Dowel) == 0);
    CHECK(quantityOf(list, HardwareKind::DrawerSlide) == 0);
    // Kinds without pieces are not listed at all.
    CHECK(std::none_of(list.begin(), list.end(), [](const HardwareItem& i) { return i.quantity == 0; }));

    const auto confirmat = std::find_if(list.begin(), list.end(), [](const HardwareItem& i) {
        return i.kind == HardwareKind::Confirmat;
    });
    REQUIRE(confirmat != list.end());
    CHECK(confirmat->spec == "7 × 50");
}

TEST_CASE("Hardware follows the workshop's fastener choice", "[detailing][hardware]")
{
    ConstructionRules rules;
    rules.joints.fastener = ConstructionRules::JointFastener::Dowel;
    const auto wardrobe = WardrobeBuilder().rules(rules).shelvesPerSection(2).build();
    applyDetailing(*wardrobe);
    const auto list = hardwareList(*wardrobe);
    CHECK(quantityOf(list, HardwareKind::Confirmat) == 0);
    CHECK(quantityOf(list, HardwareKind::Dowel) == 4 * 3);
}

TEST_CASE("Drawers bring slides and handles", "[detailing][hardware]")
{
    const auto chest = createProduct(CatalogItem::ChestOfDrawers);
    applyDetailing(*chest);
    const auto list = hardwareList(*chest);
    CHECK(quantityOf(list, HardwareKind::DrawerSlide) == 4); // pairs
    CHECK(quantityOf(list, HardwareKind::Handle) == 4);
    const auto slides = std::find_if(list.begin(), list.end(), [](const HardwareItem& i) {
        return i.kind == HardwareKind::DrawerSlide;
    });
    REQUIRE(slides != list.end());
    CHECK(slides->spec.ends_with(" mm"));
}
