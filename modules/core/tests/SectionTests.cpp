// Stage 2/3: compartments — shelves, partitions, doors, drawers.
//
// Reference interior of an 800 x 2000 x 560 carcass: x 18..782, y 18..1982, z 19..560.

#include "readycad/test/TestHelpers.hpp"

#include "readycad/core/builders/WardrobeBuilder.hpp"
#include "readycad/core/model/Drawer.hpp"
#include "readycad/core/model/Queries.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <stdexcept>
#include <vector>

using namespace rc::core;
using Catch::Approx;
using rc::test::IsBox;
using rc::test::rulesWithoutSnapping;

TEST_CASE("Automatic shelves split the height into equal clear gaps", "[core][section][shelf]")
{
    const auto wardrobe = WardrobeBuilder()
                              .rules(rulesWithoutSnapping())
                              .shelvesPerSection(4)
                              .doorsPerSection(0)
                              .build();
    const auto shelves = wardrobe->carcass()->interior().shelves();
    REQUIRE(shelves.size() == 4);

    // (1964 - 4 * 18) / 5 = 378.4
    const Real gap = 378.4;
    for (std::size_t i = 0; i < shelves.size(); ++i) {
        const auto k = static_cast<Real>(i);
        CHECK(shelves[i]->box().min().y == Approx(18.0 + (k + 1.0) * gap + k * 18.0));
    }
}

TEST_CASE("An adjustable shelf applies its own clearances", "[core][section][shelf]")
{
    const auto wardrobe = WardrobeBuilder()
                              .rules(rulesWithoutSnapping())
                              .shelvesPerSection(1)
                              .doorsPerSection(0)
                              .build();
    const Shelf& shelf = *wardrobe->carcass()->interior().shelves().front();

    // 1 mm per side, 20 mm behind the front edge, 2 mm off the back panel.
    CHECK_THAT(shelf.box(), IsBox({19, 991, 21}, {781, 1009, 540}));
    CHECK(shelf.panel()->finishLength() == 762.0);
    CHECK(shelf.panel()->finishWidth() == 519.0);
    CHECK(shelf.panel()->cutWidth() == 517.0); // 2 mm ABS on the front edge
}

TEST_CASE("Shelves with a fixed elevation anchor the automatic ones", "[core][section][shelf]")
{
    const auto wardrobe = WardrobeBuilder()
                              .rules(rulesWithoutSnapping())
                              .shelvesPerSection(0)
                              .doorsPerSection(0)
                              .build();
    Section& interior = wardrobe->carcass()->interior();
    interior.addShelf();
    interior.addShelf(1000.0); // underside 1000 mm above the section bottom
    interior.addShelf();

    const auto shelves = interior.shelves();
    CHECK(shelves[1]->box().min().y == Approx(1018.0));
    // Below the anchor: 18..1018 -> (1000 - 18) / 2 = 491 gap.
    CHECK(shelves[0]->box().min().y == Approx(509.0));
    // Above the anchor: 1036..1982 -> (946 - 18) / 2 = 464 gap.
    CHECK(shelves[2]->box().min().y == Approx(1500.0));
    CHECK(findOverlaps(*wardrobe).empty());
}

TEST_CASE("Overlapping shelves are reported by the validator", "[core][section][overlap]")
{
    const auto wardrobe = WardrobeBuilder()
                              .rules(rulesWithoutSnapping())
                              .shelvesPerSection(0)
                              .doorsPerSection(0)
                              .build();
    Section& interior = wardrobe->carcass()->interior();
    interior.addShelf(500.0);
    interior.addShelf(505.0);

    const auto overlaps = findOverlaps(*wardrobe);
    REQUIRE(overlaps.size() == 1);
    CHECK(overlaps.front().region.size().y == Approx(13.0));
}

TEST_CASE("Columns: partitions, fixed and automatic widths", "[core][section][split]")
{
    const auto wardrobe = WardrobeBuilder()
                              .dimensions({1200, 2000, 560})
                              .sections(2)
                              .sectionWidth(0, 400.0)
                              .shelvesPerSection(2)
                              .doorsPerSection(0)
                              .build();
    Section& interior = wardrobe->carcass()->interior();
    const auto columns = interior.subsections();
    REQUIRE(columns.size() == 2);

    CHECK_THAT(columns[0]->box(), IsBox({18, 18, 19}, {418, 1982, 560}));
    const auto partitions = findAll<Partition>(*wardrobe);
    REQUIRE(partitions.size() == 1);
    CHECK_THAT(partitions.front()->box(), IsBox({418, 18, 19}, {436, 1982, 560}));
    CHECK_THAT(columns[1]->box(), IsBox({436, 18, 19}, {1182, 1982, 560}));

    CHECK(findOverlaps(*wardrobe).empty());
}

TEST_CASE("Doors cover the carcass with the configured gaps", "[core][section][door]")
{
    SECTION("a pair of doors on one section")
    {
        const auto wardrobe = WardrobeBuilder().doorsPerSection(2).build();
        const auto doors = wardrobe->carcass()->interior().doors();
        REQUIRE(doors.size() == 2);

        // 1.5 mm from the outer edges, 3 mm between the doors; in front of the carcass.
        CHECK_THAT(doors[0]->box(), IsBox({1.5, 1.5, 560}, {398.5, 1998.5, 578}));
        CHECK_THAT(doors[1]->box(), IsBox({401.5, 1.5, 560}, {798.5, 1998.5, 578}));
        CHECK(doors[0]->hinge() == HingeSide::Left);
        CHECK(doors[1]->hinge() == HingeSide::Right);
        CHECK(doors[0]->panel()->finishLength() == 1997.0); // grain runs vertically
    }

    SECTION("neighbouring doors share a partition half-and-half")
    {
        const auto wardrobe = WardrobeBuilder()
                                  .dimensions({1200, 2000, 560})
                                  .sections(2)
                                  .doorsPerSection(1)
                                  .build();
        const auto doors = findAll<Door>(*wardrobe);
        REQUIRE(doors.size() == 2);
        // Columns 573 wide each; the partition 591..609 is split into 7.5 | 3 | 7.5.
        CHECK(doors[0]->box().min().x == Approx(1.5));
        CHECK(doors[0]->box().max().x == Approx(598.5));
        CHECK(doors[1]->box().min().x == Approx(601.5));
        CHECK(doors[1]->box().max().x == Approx(1198.5));
    }
}

TEST_CASE("Chest of drawers: equal fronts, boxes on standard slides", "[core][section][drawer]")
{
    const auto chest = WardrobeBuilder()
                           .dimensions({800, 850, 450})
                           .drawers(4)
                           .build();
    const DrawerStack* stack = chest->carcass()->interior().drawerStack();
    REQUIRE(stack != nullptr);
    const auto drawers = stack->drawers();
    REQUIRE(drawers.size() == 4);

    // Front area 797 x 847, three 3 mm gaps -> fronts 209.5 high.
    Real expectedBottom = 1.5;
    for (const Drawer* drawer : drawers) {
        const Aabb& front = drawer->front().box();
        CHECK(front.min().y == Approx(expectedBottom));
        CHECK(front.size().y == Approx(209.5));
        CHECK(front.size().x == Approx(797.0));
        CHECK(front.min().z == Approx(450.0));
        expectedBottom += 209.5 + 3.0;
    }

    // Lowest drawer: zone 18..211; body 738 wide (13 mm slides), 10 mm above the bottom,
    // zone height 193 - 30 = 163 high; depth 431 - 10 = 421 -> 400 mm slide.
    const Drawer& first = *drawers.front();
    CHECK(first.slideLength() == 400.0);
    CHECK_THAT(first.body(), IsBox({31, 28, 50}, {769, 191, 450}));

    // Its box parts follow their own formulas inside the body.
    const auto sides = findAll<DrawerBoxSide>(first);
    REQUIRE(sides.size() == 2);
    CHECK_THAT(sides[0]->box(), IsBox({31, 31, 50}, {47, 191, 450}));
    const auto ends = findAll<DrawerBoxEnd>(first);
    REQUIRE(ends.size() == 2);
    CHECK(ends[0]->box().size().x == Approx(706.0));
    const auto bottoms = findAll<DrawerBottom>(first);
    REQUIRE(bottoms.size() == 1);
    CHECK_THAT(bottoms[0]->box(), IsBox({31, 28, 50}, {769, 31, 450}));

    CHECK(findOverlaps(*chest).empty());
}

TEST_CASE("Drawers below doors: fronts meet at the fixed shelf with one gap",
          "[core][section][drawer]")
{
    const auto wardrobe = WardrobeBuilder()
                              .dimensions({800, 2000, 560})
                              .drawers(2, 400.0)
                              .shelvesPerSection(3)
                              .doorsPerSection(2)
                              .build();
    const auto doors = findAll<Door>(*wardrobe);
    const auto fronts = findAll<DrawerFront>(*wardrobe);
    REQUIRE(doors.size() == 2);
    REQUIRE(fronts.size() == 2);

    // Drawer compartment 18..418, fixed shelf 418..436, upper section from 436.
    const Real topDrawerFront = fronts[1]->box().max().y;
    const Real doorBottom = doors[0]->box().min().y;
    CHECK(topDrawerFront == Approx(418.0 + 7.5));
    CHECK(doorBottom == Approx(436.0 - 7.5));
    CHECK(doorBottom - topDrawerFront == Approx(3.0));

    CHECK(findOverlaps(*wardrobe).empty());
}

TEST_CASE("Sections reject inconsistent content", "[core][section]")
{
    const auto wardrobe = WardrobeBuilder().shelvesPerSection(2).doorsPerSection(0).build();
    Section& interior = wardrobe->carcass()->interior();

    CHECK_THROWS_AS(interior.setDrawers(2), std::logic_error);       // shelves already there
    CHECK_THROWS_AS(interior.splitHorizontally(2), std::logic_error); // not empty
    CHECK_THROWS_AS(interior.add(std::make_shared<SideWall>(Side::Left)), std::invalid_argument);

    interior.setAutoShelfCount(0);
    interior.splitVertically(2);
    CHECK_THROWS_AS(interior.add(std::make_shared<Partition>()), std::invalid_argument);
    CHECK_THROWS_AS(interior.setSplitAxis(Axis::X), std::logic_error);
}

TEST_CASE("Shelf mount: on supports or fixed with confirmats", "[core][section][shelf]")
{
    SECTION("a confirmat shelf fills the opening and touches the walls and the back")
    {
        const auto wardrobe = WardrobeBuilder()
                                  .shelvesPerSection(1)
                                  .shelfMount(ShelfMount::Confirmat)
                                  .doorsPerSection(0)
                                  .build();
        const Shelf& shelf = *wardrobe->carcass()->interior().shelves().front();
        CHECK(shelf.mount() == ShelfMount::Confirmat);
        CHECK_THAT(shelf.box(), IsBox({18, 991, 19}, {782, 1009, 560}));
        CHECK(shelf.panel()->finishLength() == 764.0);
        CHECK(findOverlaps(*wardrobe).empty()); // touching is not overlapping
    }

    SECTION("both mounts can be mixed in one compartment and switched later")
    {
        const auto wardrobe = WardrobeBuilder().shelvesPerSection(0).doorsPerSection(1).build();
        Section& interior = wardrobe->carcass()->interior();
        interior.addShelf();
        const auto fixed = interior.addShelf(std::nullopt, ShelfMount::Confirmat);
        interior.addShelf();

        const auto shelves = interior.shelves();
        CHECK(shelves[0]->size().width == Approx(762.0));
        CHECK(shelves[1]->size().width == Approx(764.0));
        CHECK(shelves[2]->size().width == Approx(762.0));
        // The mount does not change the even vertical distribution: (1964 - 54) / 4.
        CHECK(shelves[1]->box().min().y == Approx(18.0 + 2.0 * 477.5 + 18.0));
        CHECK(findOverlaps(*wardrobe).empty());

        fixed->setMount(ShelfMount::Supports);
        CHECK(shelves[1]->size().width == Approx(762.0));
        CHECK(shelves[1]->size().depth == Approx(519.0));
    }
}

TEST_CASE("Shelves on supports stand on the System 32 grid", "[core][section][shelf][system32]")
{
    SECTION("each shelf moves to the hole nearest to its even position")
    {
        const auto wardrobe = WardrobeBuilder().shelvesPerSection(4).doorsPerSection(0).build();
        const Carcass& c = *wardrobe->carcass();
        CHECK(c.system32OriginY() == Approx(9.0)); // centre plane of the 18 mm bottom

        // Even positions 396.4 / 792.8 / 1189.2 / 1585.6; the shelf rests 2.5 mm above a hole
        // centre at 9 + 32k.
        const std::vector<Real> expected{395.5, 779.5, 1195.5, 1579.5};
        const auto shelves = c.interior().shelves();
        REQUIRE(shelves.size() == expected.size());
        for (std::size_t i = 0; i < shelves.size(); ++i) {
            const Real underside = shelves[i]->box().min().y;
            CHECK(underside == Approx(expected[i]));
            const Real holeCentre = underside - 2.5;
            CHECK(std::remainder(holeCentre - 9.0, 32.0) == Approx(0.0).margin(1e-9));
        }
        CHECK(findOverlaps(*wardrobe).empty());
    }

    SECTION("shelves on confirmats keep their exact position")
    {
        const auto wardrobe = WardrobeBuilder()
                                  .shelvesPerSection(4)
                                  .shelfMount(ShelfMount::Confirmat)
                                  .doorsPerSection(0)
                                  .build();
        CHECK(wardrobe->carcass()->interior().shelves().front()->box().min().y
              == Approx(396.4));
    }

    SECTION("a fixed elevation snaps too")
    {
        const auto wardrobe = WardrobeBuilder().shelvesPerSection(0).doorsPerSection(0).build();
        const auto shelf = wardrobe->carcass()->interior().addShelf(1000.0);
        // Wanted underside 1018 -> hole 1015.5 -> grid holes 1001 / 1033 -> 1001 is nearer
        // -> underside 1003.5.
        CHECK(shelf->box().min().y == Approx(1003.5));
    }
}
