// Stage 6: a panel unfolded into the drawing plane (base of DXF, PDF drawings and CAM).

#include "readycad/core/builders/Catalog.hpp"
#include "readycad/core/builders/WardrobeBuilder.hpp"
#include "readycad/core/model/Carcass.hpp"
#include "readycad/core/model/Queries.hpp"
#include "readycad/detailing/CutList.hpp"
#include "readycad/detailing/Detailing.hpp"
#include "readycad/detailing/FlatPart.hpp"
#include "readycad/detailing/PanelFrame.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <glm/geometric.hpp>

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

using namespace rc::core;
using namespace rc::detailing;
using Catch::Approx;

namespace {

const Material kChipboard18{"Suntalam 18", MaterialType::Chipboard, 18.0, true};
const EdgeBand kAbs2{2.0, "ABS 2"};

/// A shelf 762 x 519 lying in the XZ plane (length X, width Z = depth, thickness Y), 2 mm ABS
/// on the front edge (L2, at z = max), with one hole of every kind.
Panel testShelf()
{
    Panel shelf(kChipboard18, Axis::Y, Axis::X);
    shelf.fitTo(Aabb::fromOriginAndSize(Vec3(0), Vec3(762, 18, 519)));
    shelf.setEdge(EdgeSide::L2, kAbs2);
    shelf.addDrilling({FastenerType::Dowel, PanelFace::Top, Vec3(100, 50, 18), 8, 12});
    shelf.addDrilling({FastenerType::Dowel, PanelFace::Bottom, Vec3(200, 60, 0), 8, 12});
    shelf.addDrilling({FastenerType::Dowel, PanelFace::EdgeL1, Vec3(300, 0, 9), 8, 30});
    shelf.addDrilling({FastenerType::Confirmat, PanelFace::EdgeW2, Vec3(762, 100, 9), 5, 50});
    return shelf;
}

const FlatHole& holeOf(const FlatPart& part, FlatSide side, std::size_t n = 0)
{
    for (const FlatHole& h : part.holes) {
        if (h.side == side && n-- == 0) {
            return h;
        }
    }
    FAIL("no such hole");
    throw; // unreachable
}

/// Where a point of the panel appears when you look at `face` from outside, computed from the
/// world geometry alone: right = length axis, up = (outward normal) x right.
Vec2 seenFrom(const PanelFrame& frame, PanelFace face, const Vec3& world)
{
    Vec3 right(0.0);
    right[index(frame.lengthAxis())] = 1.0;
    Vec3 normal(0.0);
    normal[index(frame.thicknessAxis())] = face == PanelFace::Top ? 1.0 : -1.0;
    const Vec3 up = glm::cross(normal, right);
    const int w = index(frame.widthAxis());
    const Real u = world[index(frame.lengthAxis())] - frame.box().min()[index(frame.lengthAxis())];
    const Real v = up[w] > 0.0 ? world[w] - frame.box().min()[w] : frame.box().max()[w] - world[w];
    return {u, v};
}

bool lessXY(const Vec2& a, const Vec2& b)
{
    return std::pair(std::round(a.y * 1e3), std::round(a.x * 1e3))
         < std::pair(std::round(b.y * 1e3), std::round(b.x * 1e3));
}

} // namespace

TEST_CASE("Shelf seen from the top: front edge at the bottom of the drawing", "[detailing][flat]")
{
    const Panel shelf = testShelf();
    const ConstructionRules rules;

    // Looking down at a shelf, its back (z = 0) is far from you: "up" in the view is -Z, so the
    // panel's local y (along +Z) has to be flipped.
    const FlatPart top = flatten(shelf, rules, ViewFace::Top);
    CHECK(top.viewFace == PanelFace::Top);
    CHECK(top.flipped);
    CHECK(top.length == 762.0);
    CHECK(top.width == 519.0);

    CHECK(top.edgeAt(0).side == EdgeSide::L2); // bottom of the drawing = the banded front edge
    CHECK(top.edgeAt(0).band.present());
    CHECK(top.edgeAt(1).side == EdgeSide::W2);
    CHECK(top.edgeAt(2).side == EdgeSide::L1);
    CHECK(top.edgeAt(3).side == EdgeSide::W1);

    REQUIRE(top.count(FlatSide::Near) == 1);
    REQUIRE(top.count(FlatSide::Far) == 1);
    REQUIRE(top.count(FlatSide::Edge) == 2);
    CHECK(holeOf(top, FlatSide::Near).center == Vec2(100, 519 - 50));
    CHECK(holeOf(top, FlatSide::Far).center == Vec2(200, 519 - 60));

    const FlatHole& intoL1 = holeOf(top, FlatSide::Edge, 1); // sorted by v: L1 is at the top
    CHECK(intoL1.edge == EdgeSide::L1);
    CHECK(intoL1.center == Vec2(300, 519));
    CHECK(intoL1.direction == Vec2(0, -1)); // drilled downwards into the part
    CHECK(intoL1.height == Approx(9.0));
    CHECK(intoL1.depth == 30.0);

    const FlatHole& intoW2 = holeOf(top, FlatSide::Edge, 0);
    CHECK(intoW2.edge == EdgeSide::W2);
    CHECK(intoW2.center == Vec2(762, 519 - 100));
    CHECK(intoW2.direction == Vec2(-1, 0));

    // The blank: 1 mm pre-milling minus the 2 mm band on the front edge -> 1 mm less there.
    CHECK(top.cut.min == Vec2(0, 1));
    CHECK(top.cut.max == Vec2(762, 519));
}

TEST_CASE("The same shelf seen from below is not flipped", "[detailing][flat]")
{
    const FlatPart bottom = flatten(testShelf(), ConstructionRules{}, ViewFace::Bottom);
    CHECK(bottom.viewFace == PanelFace::Bottom);
    CHECK_FALSE(bottom.flipped);
    CHECK(bottom.edgeAt(0).side == EdgeSide::L1);
    CHECK(bottom.edge(EdgeSide::L2).start == Vec2(762, 519));
    CHECK(holeOf(bottom, FlatSide::Near).center == Vec2(200, 60)); // the Bottom hole is now near
    CHECK(holeOf(bottom, FlatSide::Far).center == Vec2(100, 50));
    const FlatHole& intoL1 = holeOf(bottom, FlatSide::Edge, 0);
    CHECK(intoL1.center == Vec2(300, 0));
    CHECK(intoL1.direction == Vec2(0, 1));
    CHECK(bottom.cut.min == Vec2(0, 0));
    CHECK(bottom.cut.max == Vec2(762, 518));
}

TEST_CASE("Auto view shows the inner faces of both side walls", "[detailing][flat]")
{
    const auto wardrobe = WardrobeBuilder().shelvesPerSection(4).doorsPerSection(2).build();
    applyDetailing(*wardrobe);
    const Carcass& carcass = *wardrobe->carcass();

    // Shelf supports and hinge plates are drilled from the inside. For the left wall the inner
    // face is Top (towards +X), for the right one it is Bottom; Auto finds both.
    const FlatPart left = flatten(*carcass.leftSide().panel(), wardrobe->rules());
    const FlatPart right = flatten(*carcass.rightSide().panel(), wardrobe->rules());
    CHECK(left.viewFace == PanelFace::Top);
    CHECK(right.viewFace == PanelFace::Bottom);
    CHECK(left.count(FlatSide::Near) == right.count(FlatSide::Near));
    CHECK(left.count(FlatSide::Near) > 8);

    // The only holes on the outer face: confirmats through the wall into the bottom panel,
    // which stands between the walls.
    for (const FlatPart* wall : {&left, &right}) {
        CHECK(wall->count(FlatSide::Far) == 3);
        for (const FlatHole& h : wall->holes) {
            if (h.side == FlatSide::Far) {
                CHECK(h.fastener == FastenerType::Confirmat);
                CHECK(h.center.x < 50.0); // at the bottom end of the wall
            }
        }
    }
    // The walls are mirror parts: seen from inside, the front edge (hinge plates) is at the top
    // of the left wall's drawing and at the bottom of the right one's.
    std::vector<Vec2> mirroredLeft;
    std::vector<Vec2> rightHoles;
    for (std::size_t i = 0; i < left.holes.size(); ++i) {
        mirroredLeft.emplace_back(left.holes[i].center.x, left.width - left.holes[i].center.y);
        rightHoles.push_back(right.holes[i].center);
    }
    std::sort(mirroredLeft.begin(), mirroredLeft.end(), lessXY);
    std::sort(rightHoles.begin(), rightHoles.end(), lessXY);
    for (std::size_t i = 0; i < rightHoles.size(); ++i) {
        CHECK(nearlyEqual(mirroredLeft[i].x, rightHoles[i].x, 1e-6));
        CHECK(nearlyEqual(mirroredLeft[i].y, rightHoles[i].y, 1e-6));
    }
}

TEST_CASE("Flat views are never mirrored and the blank matches the cut size", "[detailing][flat]")
{
    for (const CatalogItem item : catalogItems()) {
        const auto product = createProduct(item);
        applyDetailing(*product);
        for (const FurnitureComponent* component : collectPanels(*product)) {
            const Panel& panel = *component->panel();
            const PanelFrame frame(*component);
            for (const ViewFace choice : {ViewFace::Top, ViewFace::Bottom}) {
                const FlatPart part = flatten(panel, product->rules(), choice);
                INFO(toString(component->kind()) << (choice == ViewFace::Top ? " top" : " bottom"));

                // Every near hole sits where a person looking at that face would see it.
                std::vector<Vec2> expected;
                for (const Drilling& d : panel.drillings()) {
                    if (d.face == part.viewFace) {
                        expected.push_back(seenFrom(frame, part.viewFace, frame.toWorld(d.position)));
                    }
                }
                std::vector<Vec2> actual;
                for (const FlatHole& h : part.holes) {
                    if (h.side == FlatSide::Near) {
                        actual.push_back(h.center);
                    }
                }
                std::sort(expected.begin(), expected.end(), lessXY);
                std::sort(actual.begin(), actual.end(), lessXY);
                REQUIRE(actual.size() == expected.size());
                for (std::size_t i = 0; i < actual.size(); ++i) {
                    CHECK(nearlyEqual(actual[i].x, expected[i].x, 1e-6));
                    CHECK(nearlyEqual(actual[i].y, expected[i].y, 1e-6));
                }

                const CutSize cut = cutSize(panel, product->rules());
                CHECK(part.cut.size().x == Approx(cut.length).margin(0.05));
                CHECK(part.cut.size().y == Approx(cut.width).margin(0.05));
            }
        }
    }
}
