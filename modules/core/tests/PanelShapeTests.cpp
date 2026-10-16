// Shapes of parts: notches, shaped corners and openings bound to the edges of a panel.
//
// Reference panel: 2000 x 560 (a side wall: length = height, width = depth; L1 the rear edge,
// W1 the bottom end).

#include "readycad/core/PanelShape.hpp"
#include "readycad/core/Polygon.hpp"
#include "readycad/core/builders/WardrobeBuilder.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <glm/geometric.hpp>

#include <stdexcept>

using namespace rc::core;
using Catch::Approx;

namespace {

constexpr Real kL = 2000.0;
constexpr Real kW = 560.0;

std::vector<Vec2> points(const std::vector<ShapeFeature>& features, std::array<Real, 4> grow = {})
{
    return outlinePoints(panelOutline(kL, kW, features, grow));
}

} // namespace

TEST_CASE("Without corner or edge features the outline is the rectangle", "[core][shape]")
{
    const auto outline = panelOutline(kL, kW, {});
    REQUIRE(outline.size() == 4);
    CHECK(outlinePoints(outline) == std::vector<Vec2>{{0, 0}, {kL, 0}, {kL, kW}, {0, kW}});
    for (const OutlineSide& side : outline) {
        CHECK(side.kind == OutlineSideKind::Edge);
    }
    CHECK(outline[0].edge == EdgeSide::L1);
    CHECK(outline[1].edge == EdgeSide::W2);
    CHECK(outline[2].edge == EdgeSide::L2);
    CHECK(outline[3].edge == EdgeSide::W1);
    // Openings do not change the outline.
    CHECK(panelOutline(kL, kW, {RoundHole{}}).size() == 4);
}

TEST_CASE("A corner notch: a step into the corner, its sides not banded", "[core][shape]")
{
    // The skirting board notch: 100 along the length (up the wall), 15 into its depth.
    const auto outline = panelOutline(kL, kW, {CornerNotch{PanelCorner::L1W1, 100.0, 15.0}});
    CHECK(outlinePoints(outline)
          == std::vector<Vec2>{{0, 15}, {100, 15}, {100, 0}, {kL, 0}, {kL, kW}, {0, kW}});
    CHECK(outline[0].kind == OutlineSideKind::Notch);
    CHECK(outline[1].kind == OutlineSideKind::Notch);
    CHECK(outline[2].kind == OutlineSideKind::Edge);
    CHECK(outline[2].edge == EdgeSide::L1);
    CHECK(signedArea(outlinePoints(outline)) == Approx(kL * kW - 100.0 * 15.0));

    // The same corner notch at the opposite corner: sizes stay along length and width.
    const auto far = points({CornerNotch{PanelCorner::L2W2, 100.0, 15.0}});
    CHECK(far == std::vector<Vec2>{{0, 0}, {kL, 0}, {kL, kW - 15}, {kL - 100, kW - 15},
                                   {kL - 100, kW}, {0, kW}});
}

TEST_CASE("Rounded and chamfered corners: banded like their edges", "[core][shape]")
{
    const auto round = panelOutline(kL, kW, {CornerRound{PanelCorner::L1W2, 50.0}}, {}, 4);
    // Four pieces of the arc from (1950, 0) to (2000, 50), then the edge W2.
    REQUIRE(round.size() == 3 + 5);
    CHECK(round[1].start == Vec2(kL - 50, 0));
    CHECK(round[1].kind == OutlineSideKind::Corner);
    CHECK(round[1].corner == PanelCorner::L1W2);
    CHECK(round[5].start.x == Approx(kL));
    CHECK(round[5].start.y == Approx(50.0));
    CHECK(round[5].kind == OutlineSideKind::Edge);
    CHECK(round[5].edge == EdgeSide::W2);
    for (const Vec2 p : outlinePoints(round)) {
        if (p.x > kL - 50 && p.y < 50) {
            CHECK(glm::length(p - Vec2(kL - 50, 50)) == Approx(50.0)); // on the arc
        }
    }

    const auto chamfer = panelOutline(kL, kW, {CornerChamfer{PanelCorner::L2W1, 80.0, 40.0}});
    CHECK(outlinePoints(chamfer)
          == std::vector<Vec2>{{0, 0}, {kL, 0}, {kL, kW}, {80, kW}, {0, kW - 40}});
    CHECK(chamfer[3].kind == OutlineSideKind::Corner);
}

TEST_CASE("An edge notch, measured from either corner of its edge", "[core][shape]")
{
    // A pipe behind the wall: a notch in the rear edge, 600 up from the bottom.
    CHECK(points({EdgeNotch{EdgeSide::L1, false, 600.0, 120.0, 80.0}})
          == std::vector<Vec2>{{0, 0}, {600, 0}, {600, 80}, {720, 80}, {720, 0}, {kL, 0},
                               {kL, kW}, {0, kW}});
    // The same from the top end, in the front edge (run backwards along the outline).
    CHECK(points({EdgeNotch{EdgeSide::L2, true, 600.0, 120.0, 80.0}})
          == std::vector<Vec2>{{0, 0}, {kL, 0}, {kL, kW}, {1400, kW}, {1400, kW - 80},
                               {1280, kW - 80}, {1280, kW}, {0, kW}});
}

TEST_CASE("The blank: banded edges grown, features in their finished place", "[core][shape]")
{
    // Pre-milling 1 mm and a 2 mm band on L2: that edge of the blank is 1 mm inside.
    std::array<Real, 4> grow{};
    grow[static_cast<std::size_t>(EdgeSide::L2)] = -1.0;
    grow[static_cast<std::size_t>(EdgeSide::W1)] = 0.6;
    const auto blank = points({CornerNotch{PanelCorner::L1W1, 100.0, 15.0}}, grow);
    CHECK(blank == std::vector<Vec2>{{-0.6, 15}, {100, 15}, {100, 0}, {kL, 0}, {kL, kW - 1},
                                     {-0.6, kW - 1}});
}

TEST_CASE("Openings: measured from a corner, a round hole is a full circle", "[core][shape]")
{
    const auto openings = featureOpenings(
        kL, kW, {InnerCutout{PanelCorner::L2W2, 300.0, 100.0, 120.0, 60.0, 5.0},
                 RoundHole{PanelCorner::L1W1, 900.0, 100.0, 60.0}, CornerRound{}});
    REQUIRE(openings.size() == 2);
    CHECK(openings[0] == PanelCutout{Vec2(kL - 300, kW - 100), 120.0, 60.0, 5.0});
    CHECK(openings[1] == PanelCutout{Vec2(900, 100), 60.0, 60.0, 30.0});

    const auto circle = openingOutline(openings[1], 8);
    CHECK(circle.size() == 32);
    for (const Vec2 p : circle) {
        CHECK(glm::length(p - Vec2(900, 100)) == Approx(30.0));
    }
    CHECK(signedArea(circle) > 0.0);
}

TEST_CASE("What fits and what does not", "[core][shape]")
{
    const auto fits = [](const std::vector<ShapeFeature>& features) {
        return shapeFits(kL, kW, features, featureOpenings(kL, kW, features), 10.0);
    };
    CHECK(fits({CornerNotch{PanelCorner::L1W1, 100.0, 15.0}, RoundHole{PanelCorner::L1W1, 900.0, 100.0, 60.0},
                CornerRound{PanelCorner::L2W2, 50.0}, EdgeNotch{EdgeSide::L1, false, 600.0, 120.0, 80.0}}));
    // Two features at one corner.
    CHECK_FALSE(fits({CornerRound{PanelCorner::L1W1, 20.0}, CornerNotch{PanelCorner::L1W1}}));
    // A notch through the panel, a corner as wide as the panel.
    CHECK_FALSE(fits({EdgeNotch{EdgeSide::L1, false, 600.0, 120.0, kW}}));
    CHECK_FALSE(fits({CornerNotch{PanelCorner::L1W1, 100.0, kW}}));
    // An edge notch running into the corner notch, or touching it.
    CHECK_FALSE(fits({CornerNotch{PanelCorner::L1W1, 100.0, 15.0}, EdgeNotch{EdgeSide::L1, false, 50.0, 100.0, 30.0}}));
    CHECK_FALSE(fits({CornerNotch{PanelCorner::L1W1, 100.0, 15.0}, EdgeNotch{EdgeSide::L1, false, 100.0, 100.0, 30.0}}));
    // Notches from opposite edges that meet.
    CHECK_FALSE(fits({EdgeNotch{EdgeSide::L1, false, 600.0, 120.0, 300.0}, EdgeNotch{EdgeSide::L2, false, 600.0, 120.0, 300.0}}));
    // An opening outside the part, too close to the edge, or too close to another.
    CHECK_FALSE(fits({RoundHole{PanelCorner::L1W1, 900.0, -10.0, 60.0}}) );
    CHECK_FALSE(fits({RoundHole{PanelCorner::L1W1, 900.0, 35.0, 60.0}})); // 5 mm of material
    CHECK(fits({RoundHole{PanelCorner::L1W1, 900.0, 40.0, 60.0}}));        // 10 mm
    CHECK_FALSE(fits({RoundHole{PanelCorner::L1W1, 900.0, 100.0, 60.0},
                      RoundHole{PanelCorner::L1W1, 960.0, 100.0, 60.0}}));
    // An opening in the corner a notch took away.
    CHECK_FALSE(fits({CornerNotch{PanelCorner::L1W1, 300.0, 200.0}, RoundHole{PanelCorner::L1W1, 100.0, 100.0, 60.0}}));
}

TEST_CASE("A part keeps its features when it changes size", "[core][shape]")
{
    const auto wardrobe = WardrobeBuilder().build();
    PanelComponent& wall = wardrobe->carcass()->leftSide();
    wall.setShape({CornerNotch{PanelCorner::L1W1, 100.0, 15.0},
                   RoundHole{PanelCorner::L2W2, 300.0, 100.0, 60.0}});
    REQUIRE(wall.panel()->outline().size() == 6);
    CHECK(wall.panel()->cutouts().front().center == Vec2(1982.0 - 300.0, 560.0 - 100.0));

    wardrobe->setDimensions({800, 2200, 600});
    // The notch stays in its corner, the hole 300 from the top end and 100 from the front.
    CHECK(wall.panel()->outline()[1].start == Vec2(100.0, 15.0));
    CHECK(wall.panel()->cutouts().front().center == Vec2(2182.0 - 300.0, 600.0 - 100.0));
    // Clearing them: a rectangle again.
    wall.setShape({});
    CHECK(wall.panel()->outline().empty());
    CHECK(wall.panel()->cutouts().empty());

    CHECK_THROWS_AS(wall.setShape({CornerRound{PanelCorner::L1W1, 0.0}}), std::invalid_argument);
    CHECK_THROWS_AS(wall.setShape({InnerCutout{PanelCorner::L1W1, 100.0, 100.0, 50.0, 40.0, 30.0}}),
                    std::invalid_argument); // corners rounder than the opening
}
