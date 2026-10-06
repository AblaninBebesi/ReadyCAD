// Stage 4: finding joints from geometry and spacing the connectors.

#include "readycad/core/builders/Catalog.hpp"
#include "readycad/core/builders/WardrobeBuilder.hpp"
#include "readycad/detailing/Joints.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <vector>

using namespace rc::core;
using namespace rc::detailing;
using Catch::Approx;

TEST_CASE("Connector positions along a joint", "[detailing][joints]")
{
    const ConstructionRules::Joints rules; // 50 from the ends, at most 300 apart

    CHECK(connectorPositions(0, 560, rules) == std::vector<Real>{50, 280, 510});
    CHECK(connectorPositions(0, 400, rules) == std::vector<Real>{50, 350});
    CHECK(connectorPositions(0, 90, rules) == std::vector<Real>{45}); // short: one in the middle
    CHECK(connectorPositions(100, 100, rules).empty());

    const auto four = connectorPositions(0, 1000, rules); // 900 between the end connectors
    REQUIRE(four.size() == 4);
    CHECK(four[1] == Approx(350.0));
}

TEST_CASE("Carcass joints are found from geometry", "[detailing][joints]")
{
    const auto wardrobe = WardrobeBuilder().shelvesPerSection(4).doorsPerSection(2).build();
    Carcass& c = *wardrobe->carcass();
    const auto joints = findJoints(*wardrobe);

    // Top on both sides, bottom between both sides. Back, doors and shelves on supports
    // are not connector joints.
    REQUIRE(joints.size() == 4);

    for (const Joint& j : joints) {
        if (j.face == &c.top()) {
            CHECK(j.normal == Axis::Y);
            CHECK(j.across == Axis::X);
            CHECK(j.along == Axis::Z);
            CHECK(j.plane == Approx(1982.0));
            CHECK(j.contact.size().z == Approx(560.0));
        } else {
            // Inset bottom: the side's face takes the bottom's end.
            CHECK(j.edge == &c.bottom());
            CHECK(j.normal == Axis::X);
            CHECK(j.across == Axis::Y);
        }
    }
}

TEST_CASE("Partitions, fixed shelves and confirmat shelves create joints", "[detailing][joints]")
{
    SECTION("a partition joins the top and the bottom")
    {
        const auto wardrobe =
            WardrobeBuilder().dimensions({1200, 2000, 560}).sections(2).build();
        CHECK(findJoints(*wardrobe).size() == 6);
    }

    SECTION("shelves on confirmats join both walls, shelves on supports do not")
    {
        const auto wardrobe = WardrobeBuilder()
                                  .shelvesPerSection(2)
                                  .shelfMount(ShelfMount::Confirmat)
                                  .build();
        CHECK(findJoints(*wardrobe).size() == 4 + 2 * 2);
    }

    SECTION("drawer boxes: each end between two sides")
    {
        const auto chest = createProduct(CatalogItem::ChestOfDrawers);
        CHECK(findJoints(*chest).size() == 4 + 4 * 4);
    }
}
