// Handles: catalogue lookup and placement on fronts.
//
// Reference wardrobe 800 x 2000 x 560 with a pair of doors:
//   left door  x 1.5..398.5,   hinged left  -> handle at its right (opening) edge
//   right door x 401.5..798.5, hinged right -> handle at its left edge
//   both       y 1.5..1998.5,  outer face z = 578

#include "readycad/test/TestHelpers.hpp"

#include "readycad/core/builders/WardrobeBuilder.hpp"
#include "readycad/core/model/Drawer.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace rc::core;
using Catch::Approx;
using rc::test::IsNear;

TEST_CASE("The default catalogue has bars on the 32 mm module and a knob", "[core][handle]")
{
    const ConstructionRules rules;
    const HandleModel* bar = rules.handles.find("bar-128");
    REQUIRE(bar != nullptr);
    CHECK(bar->type == HandleType::Bar);
    CHECK(bar->holeSpacing == 128.0);
    CHECK(bar->holeCount() == 2);

    const HandleModel* knob = rules.handles.find("knob-30");
    REQUIRE(knob != nullptr);
    CHECK(knob->holeCount() == 1);

    CHECK(rules.handles.find("no-such-handle") == nullptr);
}

TEST_CASE("Door handle: vertical, mid-height, at the opening edge", "[core][handle]")
{
    const auto wardrobe = WardrobeBuilder().doorsPerSection(2).build();
    const auto doors = wardrobe->carcass()->interior().doors();
    const ConstructionRules& rules = wardrobe->rules();

    const auto left = doors[0]->handleGeometry(rules);
    REQUIRE(left.has_value());
    CHECK(left->model->id == "bar-128");
    CHECK(left->along == Axis::Y);
    CHECK(left->fits);
    REQUIRE(left->holes.size() == 2);
    CHECK_THAT(left->holes[0], IsNear({358.5, 1000.0 - 64.0, 578.0}));
    CHECK_THAT(left->holes[1], IsNear({358.5, 1000.0 + 64.0, 578.0}));

    const auto right = doors[1]->handleGeometry(rules);
    REQUIRE(right.has_value());
    CHECK(right->center.x == Approx(441.5));

    SECTION("re-hanging the door moves the handle to the other edge")
    {
        doors[0]->setHinge(HingeSide::Right);
        CHECK(doors[0]->handleGeometry(rules)->center.x == Approx(41.5));
    }
}

TEST_CASE("Handle placement options", "[core][handle]")
{
    const auto wardrobe = WardrobeBuilder().doorsPerSection(2).build();
    Door& door = *wardrobe->carcass()->interior().doors()[0];
    const ConstructionRules& rules = wardrobe->rules();

    SECTION("top anchor: nearest hole 60 mm below the top edge")
    {
        door.setHandle(HandlePlacement{"bar-128", HandleOrientation::Vertical, HandleAnchor::Top,
                                       40.0, 60.0});
        const auto g = door.handleGeometry(rules);
        CHECK(g->holes[1].y == Approx(1998.5 - 60.0));
        CHECK(g->holes[0].y == Approx(1998.5 - 60.0 - 128.0));
    }

    SECTION("horizontal bar: nearest hole 40 mm from the opening edge")
    {
        door.setHandle(HandlePlacement{"bar-128", HandleOrientation::Horizontal});
        const auto g = door.handleGeometry(rules);
        CHECK(g->along == Axis::X);
        CHECK(g->holes[1].x == Approx(358.5));
        CHECK(g->holes[0].x == Approx(358.5 - 128.0));
    }

    SECTION("no handle, unknown model")
    {
        door.setHandle(std::nullopt);
        CHECK_FALSE(door.handleGeometry(rules).has_value());

        door.setHandle(HandlePlacement{"discontinued-handle"});
        CHECK(door.handleModel(rules) == nullptr);
        CHECK_FALSE(door.handleGeometry(rules).has_value());
    }
}

TEST_CASE("Drawer fronts: horizontal and centred; knobs; fit check", "[core][handle]")
{
    const auto chest = WardrobeBuilder().dimensions({800, 850, 450}).drawers(4).build();
    DrawerFront& front = chest->carcass()->interior().drawerStack()->drawers()[0]->front();
    const ConstructionRules& rules = chest->rules();

    // Lowest front: x 1.5..798.5, y 1.5..211, outer face z = 468.
    const auto bar = front.handleGeometry(rules);
    REQUIRE(bar.has_value());
    CHECK(bar->along == Axis::X);
    CHECK_THAT(bar->holes[0], IsNear({336.0, 106.25, 468.0}));
    CHECK_THAT(bar->holes[1], IsNear({464.0, 106.25, 468.0}));

    front.setHandle(HandlePlacement{"knob-30", HandleOrientation::Horizontal});
    const auto knob = front.handleGeometry(rules);
    REQUIRE(knob->holes.size() == 1);
    CHECK_THAT(knob->holes[0], IsNear({400.0, 106.25, 468.0}));

    // A 320 mm bar standing upright does not fit on a 209.5 mm high front.
    front.setHandle(HandlePlacement{"bar-320", HandleOrientation::Vertical});
    CHECK_FALSE(front.handleGeometry(rules)->fits);
}

TEST_CASE("The workshop's default handle is used by every front without its own",
          "[core][handle]")
{
    ConstructionRules rules;
    rules.handles.defaultDoorHandle = "bar-192";
    const auto wardrobe = WardrobeBuilder().rules(rules).doorsPerSection(1).build();
    const auto g =
        wardrobe->carcass()->interior().doors().front()->handleGeometry(wardrobe->rules());
    CHECK(g->model->holeSpacing == 192.0);
}
