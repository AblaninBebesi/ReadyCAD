// Stage 4: hole coordinates of the reference wardrobe 800 x 2000 x 560, 18 mm chipboard:
// 4 shelves on supports (snapped to System 32), 2 doors, back in a groove, confirmats.
//
// Panel coordinates: x along the length, y along the width, z across the thickness.
//   side wall: x = height (Y), y = depth (Z), z = X     top/bottom: x = X, y = depth (Z), z = Y
//   door:      x = height (Y), y = width (X), z = Z

#include "readycad/test/TestHelpers.hpp"

#include "readycad/core/builders/Catalog.hpp"
#include "readycad/core/builders/WardrobeBuilder.hpp"
#include "readycad/detailing/Detailing.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

using namespace rc::core;
using namespace rc::detailing;
using Catch::Approx;
using rc::test::IsNear;

namespace {

std::vector<Drilling> holes(const FurnitureComponent& part, FastenerType type,
                            std::optional<PanelFace> face = std::nullopt)
{
    std::vector<Drilling> result;
    for (const Drilling& d : part.panel()->drillings()) {
        if (d.fastener == type && (!face || d.face == *face)) {
            result.push_back(d);
        }
    }
    std::sort(result.begin(), result.end(), [](const Drilling& a, const Drilling& b) {
        return a.position.x != b.position.x ? a.position.x < b.position.x
                                            : a.position.y < b.position.y;
    });
    return result;
}

bool onGrid(Real y, Real origin = 9.0, Real pitch = 32.0)
{
    return std::abs(std::remainder(y - origin, pitch)) < 1e-6;
}

std::shared_ptr<Product> referenceWardrobe(ConstructionRules rules = {})
{
    return WardrobeBuilder().rules(rules).shelvesPerSection(4).doorsPerSection(2).build();
}

} // namespace

TEST_CASE("Report of the reference wardrobe", "[detailing][drilling]")
{
    const auto wardrobe = referenceWardrobe();
    const DetailingReport report = applyDetailing(*wardrobe);

    CHECK(report.joints == 4);
    CHECK(report.grooves == 4);
    // top 6 + bottom 6 + each side (3 + 3 confirmat, 8 shelf supports, 8 hinge plate) + 8 cups
    CHECK(report.drillings == 6 + 6 + 2 * 22 + 8);
    CHECK(report.warnings.empty());
}

TEST_CASE("Confirmats: overlay top on the sides", "[detailing][drilling][confirmat]")
{
    const auto wardrobe = referenceWardrobe();
    applyDetailing(*wardrobe);
    const Carcass& c = *wardrobe->carcass();

    // Top: through holes 7 mm from its outer (upper) face, above the middle of each side.
    const auto top = holes(c.top(), FastenerType::Confirmat);
    REQUIRE(top.size() == 6);
    const std::vector<Vec3> expected{{9, 50, 18},  {9, 280, 18},  {9, 510, 18},
                                     {791, 50, 18}, {791, 280, 18}, {791, 510, 18}};
    for (std::size_t i = 0; i < top.size(); ++i) {
        CHECK(top[i].face == PanelFace::Top);
        CHECK_THAT(top[i].position, IsNear(expected[i]));
        CHECK(top[i].diameter == 7.0);
        CHECK(top[i].depth == 18.0); // through
    }

    // Left side: 5 mm holes in its upper end, in the middle of its thickness, 50 - 18 + 2 deep.
    const auto sideEnd = holes(c.leftSide(), FastenerType::Confirmat, PanelFace::EdgeW2);
    REQUIRE(sideEnd.size() == 3);
    CHECK_THAT(sideEnd[0].position, IsNear({1982, 50, 9}));
    CHECK(sideEnd[0].diameter == 5.0);
    CHECK(sideEnd[0].depth == 34.0);
}

TEST_CASE("Confirmats: inset bottom between the sides", "[detailing][drilling][confirmat]")
{
    const auto wardrobe = referenceWardrobe();
    applyDetailing(*wardrobe);
    const Carcass& c = *wardrobe->carcass();

    // Through the left side from its outer face (x = 0), at the bottom's mid-thickness (y = 9).
    const auto side = holes(c.leftSide(), FastenerType::Confirmat, PanelFace::Bottom);
    REQUIRE(side.size() == 3);
    CHECK_THAT(side[1].position, IsNear({9, 280, 0}));
    CHECK(side[1].depth == 18.0);

    // The right side is drilled from ITS outer face, which is its Top (x = 800).
    CHECK(holes(c.rightSide(), FastenerType::Confirmat, PanelFace::Top).size() == 3);

    // Bottom: both ends.
    CHECK_THAT(holes(c.bottom(), FastenerType::Confirmat, PanelFace::EdgeW1)[0].position,
               IsNear({0, 50, 9}));
    CHECK_THAT(holes(c.bottom(), FastenerType::Confirmat, PanelFace::EdgeW2)[0].position,
               IsNear({764, 50, 9}));
}

TEST_CASE("Shelf supports on the System 32 grid", "[detailing][drilling][system32]")
{
    const auto wardrobe = referenceWardrobe();
    applyDetailing(*wardrobe);
    const Carcass& c = *wardrobe->carcass();

    const auto left = holes(c.leftSide(), FastenerType::ShelfSupport);
    REQUIRE(left.size() == 8); // 4 shelves x 2 rows

    const std::vector<Real> holeHeights{393, 777, 1193, 1577}; // shelf undersides - 2.5
    for (std::size_t i = 0; i < left.size(); ++i) {
        const Drilling& d = left[i];
        CHECK(d.face == PanelFace::Top); // the left side's inner face
        CHECK(d.position.x == Approx(holeHeights[i / 2]));
        CHECK(onGrid(d.position.x));
        CHECK(d.diameter == 5.0);
        CHECK(d.depth == 10.0);
    }
    // Rows: 37 mm behind the front edge (560 - 37 = 523) and 14 pitches behind it.
    CHECK(left[0].position.y == Approx(75.0));
    CHECK(left[1].position.y == Approx(523.0));

    const auto right = holes(c.rightSide(), FastenerType::ShelfSupport);
    REQUIRE(right.size() == 8);
    CHECK(right[0].face == PanelFace::Bottom); // the right side's inner face is at x = 782
    CHECK(right[0].position.z == 0.0);
}

TEST_CASE("Hinges: cups in the doors, plates on the grid", "[detailing][drilling][hinge]")
{
    const auto wardrobe = referenceWardrobe();
    applyDetailing(*wardrobe);
    const Carcass& c = *wardrobe->carcass();
    const auto doors = c.interior().doors();

    // 1997 mm door -> 4 hinges; axes snapped so that the plate holes (axis +- 16) are on the grid.
    const std::vector<Real> axes{89, 697, 1305, 1913};

    const auto cups = holes(*doors[0], FastenerType::Hinge);
    REQUIRE(cups.size() == 4);
    for (std::size_t i = 0; i < cups.size(); ++i) {
        CHECK(cups[i].face == PanelFace::Bottom); // inner face of the door
        CHECK_THAT(cups[i].position, IsNear({axes[i] - 1.5, 22.5, 0}));
        CHECK(cups[i].diameter == 35.0);
        CHECK(cups[i].depth == 13.0);
    }

    // Right door is hinged on the right: the cup is 22.5 mm from ITS right edge.
    const auto rightCups = holes(*doors[1], FastenerType::Hinge);
    REQUIRE(rightCups.size() == 4);
    CHECK(rightCups[0].position.y == Approx(397.0 - 22.5));

    const auto plates = holes(c.leftSide(), FastenerType::Hinge);
    REQUIRE(plates.size() == 8);
    for (const Drilling& d : plates) {
        CHECK(d.face == PanelFace::Top);
        CHECK(onGrid(d.position.x));
        CHECK(d.position.y == Approx(523.0)); // System 32 front row
    }
    CHECK(plates[0].position.x == Approx(89.0 - 16.0));
    CHECK(plates[1].position.x == Approx(89.0 + 16.0));
}

TEST_CASE("Grooves for the back panel", "[detailing][groove]")
{
    SECTION("groove mount: sides, top and bottom")
    {
        const auto wardrobe = referenceWardrobe();
        applyDetailing(*wardrobe);
        const Carcass& c = *wardrobe->carcass();

        REQUIRE(c.leftSide().panel()->grooves().size() == 1);
        const Groove& side = c.leftSide().panel()->grooves().front();
        CHECK(side.face == PanelFace::Top);
        CHECK(side.start.y == Approx(18.0)); // 16 from the rear + half of the 4 mm groove
        CHECK(side.start.x == Approx(10.0)); // starts where the back (11.5) minus 1.5 gap is
        CHECK(side.end.x == Approx(1982.0)); // runs out under the overlay top
        CHECK(side.width == 4.0);
        CHECK(side.depth == 8.0);

        // The overlay top gets a stopped groove: its visible ends stay clean.
        const Groove& top = c.top().panel()->grooves().front();
        CHECK(top.face == PanelFace::Bottom);
        CHECK(top.start.x == Approx(10.0));
        CHECK(top.end.x == Approx(790.0));

        CHECK(c.rightSide().panel()->grooves().front().face == PanelFace::Bottom);
        CHECK(c.bottom().panel()->grooves().front().face == PanelFace::Top);
    }

    SECTION("nailed-on back: no grooves")
    {
        const auto wardrobe = WardrobeBuilder().backMount(BackMountType::Overlay).build();
        CHECK(applyDetailing(*wardrobe).grooves == 0);
    }
}

TEST_CASE("Alternative fasteners", "[detailing][drilling]")
{
    ConstructionRules rules;

    SECTION("eccentric: bolt in the top, channel and cam in the side")
    {
        rules.joints.fastener = ConstructionRules::JointFastener::Eccentric;
        const auto wardrobe = referenceWardrobe(rules);
        applyDetailing(*wardrobe);
        const Carcass& c = *wardrobe->carcass();

        const auto bolts = holes(c.top(), FastenerType::Eccentric);
        REQUIRE(bolts.size() == 6);
        CHECK(bolts[0].face == PanelFace::Bottom); // the face the side touches
        CHECK_THAT(bolts[0].position, IsNear({9, 50, 0}));
        CHECK(bolts[0].diameter == 5.0);

        const auto channel = holes(c.leftSide(), FastenerType::Eccentric, PanelFace::EdgeW2);
        REQUIRE(channel.size() == 3);
        CHECK(channel[0].diameter == 8.0);
        CHECK(channel[0].depth == 34.0);

        // Cam: 34 mm below the side's upper end, on its inner face. (That face also carries the
        // bolt holes of the bottom joint, where the side is the face panel.)
        auto cams = holes(c.leftSide(), FastenerType::Eccentric, PanelFace::Top);
        std::erase_if(cams, [](const Drilling& d) { return d.diameter != 15.0; });
        REQUIRE(cams.size() == 3);
        CHECK_THAT(cams[0].position, IsNear({1982 - 34, 50, 18}));
        CHECK(cams[0].diameter == 15.0);
        CHECK(cams[0].depth == 12.5);
    }

    SECTION("dowels")
    {
        rules.joints.fastener = ConstructionRules::JointFastener::Dowel;
        const auto wardrobe = referenceWardrobe(rules);
        applyDetailing(*wardrobe);
        const Carcass& c = *wardrobe->carcass();

        const auto face = holes(c.top(), FastenerType::Dowel);
        REQUIRE(face.size() == 6);
        CHECK(face[0].face == PanelFace::Bottom);
        CHECK(face[0].depth == 12.0);
        const auto edge = holes(c.leftSide(), FastenerType::Dowel, PanelFace::EdgeW2);
        REQUIRE(edge.size() == 3);
        CHECK(edge[0].depth == 19.0); // 30 - 12 + 1
    }

    SECTION("confirmats with alignment dowels 32 mm towards the middle")
    {
        rules.joints.alignmentDowels = true;
        const auto wardrobe = referenceWardrobe(rules);
        applyDetailing(*wardrobe);
        const auto dowels = holes(wardrobe->carcass()->top(), FastenerType::Dowel);
        REQUIRE(dowels.size() == 6);
        CHECK(dowels[0].position.y == Approx(82.0));
        CHECK(dowels[1].position.y == Approx(248.0));
        CHECK(dowels[2].position.y == Approx(478.0));
    }
}

TEST_CASE("Partitions: holes from both faces leave material in between",
          "[detailing][drilling][partition]")
{
    const auto wardrobe = WardrobeBuilder()
                              .dimensions({1200, 2000, 560})
                              .sections(2)
                              .shelvesPerSection(3)
                              .doorsPerSection(0)
                              .build();
    applyDetailing(*wardrobe);

    const auto partition = findAll<Partition>(*wardrobe).front();
    const auto supports = holes(*partition, FastenerType::ShelfSupport);
    REQUIRE(supports.size() == 2 * 3 * 2); // both faces, 3 shelves, 2 rows
    for (const Drilling& d : supports) {
        CHECK(d.depth == Approx(8.5)); // (18 - 1) / 2
    }
    const auto& side = wardrobe->carcass()->leftSide();
    CHECK(holes(side, FastenerType::ShelfSupport).front().depth == 10.0);
}

TEST_CASE("Fixed shelves on both sides of a partition do not share connector holes",
          "[detailing][drilling][partition]")
{
    const auto product = createProduct(CatalogItem::WardrobeWithDrawers);
    applyDetailing(*product);
    const auto partition = findAll<Partition>(*product).front();

    const auto& drillings = partition->panel()->drillings();
    for (std::size_t i = 0; i < drillings.size(); ++i) {
        for (std::size_t j = i + 1; j < drillings.size(); ++j) {
            const Drilling& a = drillings[i];
            const Drilling& b = drillings[j];
            if (!a.isFaceHole() || !b.isFaceHole()) {
                continue;
            }
            const bool through = a.depth >= 18.0 || b.depth >= 18.0;
            if (a.face != b.face && !through) {
                continue; // blind holes from opposite faces are checked by depth elsewhere
            }
            const Real distance = std::hypot(a.position.x - b.position.x,
                                             a.position.y - b.position.y);
            INFO("holes " << i << " and " << j);
            CHECK(distance >= (a.diameter + b.diameter) / 2.0);
        }
    }
}

TEST_CASE("Detailing is repeatable and invalidated by relayout", "[detailing][drilling]")
{
    const auto wardrobe = referenceWardrobe();
    const int first = applyDetailing(*wardrobe).drillings;
    CHECK(applyDetailing(*wardrobe).drillings == first);

    wardrobe->setDimensions({900, 2000, 560});
    CHECK(wardrobe->carcass()->top().panel()->drillings().empty());
    CHECK(applyDetailing(*wardrobe).drillings == first); // same structure, new coordinates
    CHECK_THAT(holes(wardrobe->carcass()->top(), FastenerType::Confirmat).back().position,
               IsNear({891, 510, 18}));
}
