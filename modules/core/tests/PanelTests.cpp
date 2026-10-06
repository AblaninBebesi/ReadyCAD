#include "readycad/core/Panel.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <stdexcept>

using namespace rc::core;

namespace {
const Material kChipboard18{"Suntalam 18", MaterialType::Chipboard, 18.0, true};
}

TEST_CASE("Panel maps world axes to its length, width and thickness", "[core][panel]")
{
    // A side wall: thickness across X, length along Y (height), width along Z (depth).
    Panel side(kChipboard18, Axis::X, Axis::Y);
    CHECK(side.widthAxis() == Axis::Z);

    side.fitTo(Aabb::fromOriginAndSize(Vec3(0, 0, 0), Vec3(18, 1982, 560)));
    CHECK(side.finishLength() == 1982.0);
    CHECK(side.finishWidth() == 560.0);
    CHECK(side.thickness() == 18.0);
}

TEST_CASE("Cut size is the finish size minus the edge banding", "[core][panel]")
{
    Panel shelf(kChipboard18, Axis::Y, Axis::X);
    shelf.fitTo(Aabb::fromOriginAndSize(Vec3(0), Vec3(762, 18, 519)));

    SECTION("no edges: cut == finish")
    {
        CHECK(shelf.cutLength() == 762.0);
        CHECK(shelf.cutWidth() == 519.0);
    }

    SECTION("2 mm ABS on the front edge, 0.4 mm on both ends")
    {
        shelf.setEdge(EdgeSide::L2, {2.0, "ABS 2"});
        shelf.setEdge(EdgeSide::W1, {0.4, "PVC 0.4"});
        shelf.setEdge(EdgeSide::W2, {0.4, "PVC 0.4"});

        CHECK(shelf.cutWidth() == Catch::Approx(517.0));  // 519 - 2
        CHECK(shelf.cutLength() == Catch::Approx(761.2)); // 762 - 0.4 - 0.4
        CHECK(shelf.edge(EdgeSide::L2).present());
        CHECK_FALSE(shelf.edge(EdgeSide::L1).present());
    }
}

TEST_CASE("Panel rejects identical thickness and length axes", "[core][panel]")
{
    CHECK_THROWS_AS(Panel(kChipboard18, Axis::X, Axis::X), std::invalid_argument);
}

TEST_CASE("Panel keeps a list of drillings", "[core][panel]")
{
    Panel side(kChipboard18, Axis::X, Axis::Y);
    side.addDrilling({FastenerType::ShelfSupport, PanelFace::Top, Vec3(37, 37, 18), 5.0, 10.0});
    side.addDrilling({FastenerType::Confirmat, PanelFace::Top, Vec3(9, 50, 18), 7.0, 18.0});
    REQUIRE(side.drillings().size() == 2);
    CHECK(side.drillings()[1].fastener == FastenerType::Confirmat);

    side.clearDrillings();
    CHECK(side.drillings().empty());
}
