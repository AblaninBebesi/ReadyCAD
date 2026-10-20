// Corner kitchens: runs on the side walls in coordinates of their own, placed by quarter turns;
// blind corner cabinets at the ends of the back run; worktops meeting in a butt joint; plinths
// turning the corner.
//
// Reference: the catalogue's "Mutfak (L)" — back wall 3000, right wall 2400, 2120 high, base
// cabinets 560 deep on a 100 mm plinth (setback 40), 18 mm fronts (edge gap 1.5, gap 3), worktop
// 600 deep, wall cabinets 320 deep from 1400. The side run's cabinets start 560 + 18 = 578 from
// the back wall (338 for the wall cabinets); the corner cabinets are 1100 and 800 wide with a
// 50 mm filler.

#include "readycad/test/TestHelpers.hpp"

#include "readycad/core/builders/Catalog.hpp"
#include "readycad/core/builders/KitchenBuilder.hpp"
#include "readycad/core/model/Product.hpp"
#include "readycad/core/model/Queries.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace rc::core;
using Catch::Approx;
using rc::test::IsBox;

namespace {

void requireNoOverlaps(const FurnitureComponent& root)
{
    const auto overlaps = findOverlaps(root);
    for (const Overlap& o : overlaps) {
        UNSCOPED_INFO(toString(o.first->kind()) << " overlaps " << toString(o.second->kind()));
    }
    CHECK(overlaps.empty());
}

} // namespace

TEST_CASE("Quarter turns: boxes stay boxes, axes swap, placements nest", "[core][placement]")
{
    const Placement left{1, Vec3(0.0, 0.0, 2000.0)}; // a run on the left wall, 2000 long
    CHECK(left.point(Vec3(0.0, 5.0, 0.0)) == Vec3(0.0, 5.0, 2000.0));
    CHECK(left.point(Vec3(2000.0, 5.0, 0.0)) == Vec3(0.0, 5.0, 0.0)); // its end: the back wall
    CHECK(left.direction(Vec3(0.0, 0.0, 1.0)) == Vec3(1.0, 0.0, 0.0)); // its front faces the room
    CHECK_THAT(left.box(Aabb(Vec3(100, 0, 0), Vec3(500, 820, 560))),
               IsBox({0, 0, 1500}, {560, 820, 1900}));
    CHECK(left.axis(Axis::X) == Axis::Z);
    CHECK(left.axis(Axis::Y) == Axis::Y);

    const Placement right{3, Vec3(3000.0, 0.0, 0.0)};
    CHECK(right.direction(Vec3(0.0, 0.0, 1.0)) == Vec3(-1.0, 0.0, 0.0));
    CHECK(right.point(Vec3(0.0, 0.0, 0.0)) == Vec3(3000.0, 0.0, 0.0)); // its start: the back wall

    // A turn back is no turn at all; nested placements add up.
    const Placement back = right.within(Placement{1, Vec3(0.0)});
    CHECK(back.quarterTurns == 0);
    CHECK(back.point(Vec3(10.0, 0.0, 0.0)) == left.direction(right.point(Vec3(10.0, 0.0, 0.0))));
    CHECK(Placement{}.isIdentity());
    CHECK_FALSE(left.isIdentity());
}

TEST_CASE("An L kitchen: the side run stands on its wall, in front of the blind corner",
          "[core][kitchen][corner]")
{
    const auto product = createProduct(CatalogItem::KitchenL);
    const Kitchen& k = *product->kitchen();
    REQUIRE(k.runs().size() == 2);
    REQUIRE(k.sideRun(Side::Left) == nullptr);
    const KitchenRun& side = *k.sideRun(Side::Right);
    const KitchenCorner& corner = *k.backRun().corner(Side::Right);
    CHECK(k.backRun().corner(Side::Left) == nullptr);
    // In their own coordinates both runs start at the origin and would overlap: the check works
    // in the product's.
    requireNoOverlaps(*product);

    // The corner cabinets at the end of the back run, as wide as they are.
    const Carcass& base = corner.baseCabinet();
    CHECK_THAT(base.box(), IsBox({1900, 0, 0}, {3000, 820, 560}));
    CHECK_THAT(corner.wallCabinet().box(), IsBox({2200, 1400, 0}, {3000, 2120, 320}));
    // The rows of the back run stop there.
    CHECK(k.baseRow().cabinets().back()->box().max().x == Approx(1900.0));
    CHECK(k.wallRow().cabinets().back()->box().max().x == Approx(2200.0));

    // Blind panel: what the side run hides (578) and the filler (50), flush with the door; the
    // door stops a front gap before it and is hung on the open side.
    const BlindPanel* blind = base.blindPanel();
    REQUIRE(blind != nullptr);
    CHECK_THAT(blind->box(), IsBox({2372, 101.5, 560}, {2998.5, 818.5, 578}));
    const Door& door = *base.interior().doors().at(0);
    CHECK(door.box().min().x == Approx(1901.5));
    CHECK(door.box().max().x == Approx(2369.0));
    CHECK(door.hinge() == HingeSide::Left);
    CHECK(corner.wallCabinet().blindPanel()->box().min().x == Approx(3000.0 - 338.0 - 50.0));

    // The side run in its own coordinates: its first cabinets start in front of the corner...
    CHECK(side.baseRow().cabinets().front()->box().min().x == Approx(578.0));
    CHECK(side.wallRow().cabinets().front()->box().min().x == Approx(338.0));
    // ...and turned onto the right wall: depth along -X, length along +Z.
    CHECK_THAT(side.baseRow().cabinets().front()->productBox(), IsBox({2440, 0, 578}, {3000, 820, 978}));
    CHECK(side.placement() == Placement{3, Vec3(3000.0, 0.0, 0.0)});

    // Worktops: the back one runs on to the right wall, the side one meets its front edge.
    CHECK_THAT(k.worktop().productBox(), IsBox({0, 820, 0}, {3000, 858, 600}));
    CHECK_THAT(side.worktop().productBox(), IsBox({2400, 820, 600}, {3000, 858, 2400}));
    // No edge band where a worktop meets the wall or the other one.
    CHECK(k.worktop().panel()->edge(EdgeSide::W1).present());
    CHECK_FALSE(k.worktop().panel()->edge(EdgeSide::W2).present());
    CHECK_FALSE(side.worktop().panel()->edge(EdgeSide::W1).present());
    CHECK(side.worktop().panel()->edge(EdgeSide::W2).present());

    // Plinths turn the corner: the corner cabinet's runs to the back of the side run's
    // (560 - 40 - 18 = 502 from the right wall), the side run's reaches back to its face.
    CHECK_THAT(base.plinth()->box(), IsBox({1900, 0, 502}, {2498, 100, 520}));
    const Plinth& sidePlinth = *side.baseRow().cabinets().front()->plinth();
    CHECK_THAT(sidePlinth.productBox(), IsBox({2480, 0, 520}, {2498, 100, 978}));
}

TEST_CASE("A U kitchen: two corners, the left run ends at the back wall", "[core][kitchen][corner]")
{
    const auto product = createProduct(CatalogItem::KitchenU);
    const Kitchen& k = *product->kitchen();
    REQUIRE(k.runs().size() == 3);
    requireNoOverlaps(*product);
    const KitchenRun& left = *k.sideRun(Side::Left);
    CHECK(left.placement() == Placement{1, Vec3(0.0, 0.0, 2000.0)});
    // Its cabinets stop in front of the left corner, its worktop at the back worktop's edge.
    CHECK(left.baseRow().cabinets().back()->box().max().x == Approx(2000.0 - 578.0));
    CHECK_THAT(left.worktop().productBox(), IsBox({0, 820, 600}, {600, 858, 2000}));
    CHECK_FALSE(left.worktop().panel()->edge(EdgeSide::W2).present());
    // The left corner's door hangs on its right, away from the corner.
    const KitchenCorner& corner = *k.backRun().corner(Side::Left);
    CHECK(corner.baseCabinet().box().min().x == Approx(0.0));
    CHECK(corner.baseCabinet().interior().doors().at(0)->hinge() == HingeSide::Right);
    CHECK(corner.baseCabinet().blindPanel()->box().min().x == Approx(1.5));
    // The back worktop runs from wall to wall: no band at either end.
    CHECK_FALSE(k.worktop().panel()->edge(EdgeSide::W1).present());
    CHECK_FALSE(k.worktop().panel()->edge(EdgeSide::W2).present());

    // Resizing keeps the corners in place and everything collision-free.
    product->setDimensions({3600, 2200, 580});
    requireNoOverlaps(*product);
    CHECK(k.sideRun(Side::Right)->placement().offset.x == Approx(3600.0));
    CHECK(k.sideRun(Side::Right)->baseRow().cabinets().front()->box().min().x == Approx(598.0));
}

TEST_CASE("Side runs come and go with their corners", "[core][kitchen][corner]")
{
    const auto product = createProduct(CatalogItem::Kitchen);
    Kitchen& k = *product->kitchen();
    auto run = std::make_shared<KitchenRun>(KitchenWall::Left);
    run->setLength(1600);
    run->baseRow().add(makeCabinet(CabinetModule::BaseDoors, std::nullopt, 600));
    CHECK_THROWS_AS(k.setSideRun(Side::Right, run, makeCorner(Side::Right)), std::invalid_argument);
    CHECK_THROWS_AS(k.setSideRun(Side::Left, run, makeCorner(Side::Right)), std::invalid_argument);
    CHECK_THROWS_AS(k.setSideRun(Side::Left, run, nullptr), std::invalid_argument);
    CHECK_THROWS_AS(run->setLength(0.0), std::invalid_argument);

    product->setDimensions({3400, 2120, 560});
    k.setSideRun(Side::Left, run, makeCorner(Side::Left));
    REQUIRE(k.runs().size() == 2);
    CHECK(k.backRun().corner(Side::Left) != nullptr);
    CHECK(k.baseRow().cabinets().front()->box().min().x == Approx(1100.0));
    CHECK(k.backRun().cabinets().size() == 5 + 5 + 2);
    requireNoOverlaps(*product);

    k.setSideRun(Side::Left, nullptr, nullptr);
    CHECK(k.runs().size() == 1);
    CHECK(k.backRun().corner(Side::Left) == nullptr);
    CHECK(k.baseRow().cabinets().front()->box().min().x == Approx(0.0));
    CHECK(run->parent() == nullptr);
}

TEST_CASE("Corners stay clear when the kitchen changes", "[core][kitchen][corner]")
{
    auto product = KitchenBuilder()
                       .length(3000)
                       .sideWall(Side::Left, 2000)
                       .sideWall(Side::Right, 2000)
                       .add(CabinetModule::BaseDoors)
                       .add(CabinetModule::BaseDoors, std::nullopt, KitchenWall::Left)
                       .add(CabinetModule::BaseDoors, std::nullopt, KitchenWall::Right)
                       .add(CabinetModule::WallDoors, std::nullopt, KitchenWall::Right)
                       .build();
    requireNoOverlaps(*product);
    // A filler wider than the corner cabinet: its blind panel still stays on the cabinet.
    Carcass& base = product->kitchen()->backRun().corner(Side::Right)->baseCabinet();
    base.setBlindCorner(5000.0);
    CHECK(base.blindPanel()->box().min().x == Approx(base.box().min().x));
    base.setBlindCorner(50.0);

    // Wall cabinets as deep as the base ones: the side run's wall cabinets start further away
    // (560 + 18), still clear of the corner.
    Kitchen::Sizes sizes = product->kitchen()->sizes();
    sizes.wallDepth = 560.0;
    product->kitchen()->setSizes(sizes);
    requireNoOverlaps(*product);
    CHECK(product->kitchen()->sideRun(Side::Right)->wallRow().cabinets().front()->box().min().x
          == Approx(578.0));
}

TEST_CASE("A worktop longer than a slab is made of pieces at its cabinets' joints",
          "[core][kitchen][worktop]")
{
    // 4800 mm: doors 1200, sink 600, drawers 600, doors (automatic, 2400).
    auto product = KitchenBuilder()
                       .length(4800)
                       .add(CabinetModule::BaseDoors, 1200)
                       .add(CabinetModule::Sink, 600)
                       .add(CabinetModule::BaseDrawers, 600)
                       .add(CabinetModule::BaseDoors)
                       .build();
    Kitchen& k = *product->kitchen();
    const ConstructionRules& rules = product->rules();
    REQUIRE(k.backRun().worktops().size() == 1);
    REQUIRE(k.longWorktops(rules).size() == 1); // 4800 > 4100: a joint is needed

    // A joint at the right side of the drawers: two pieces, both from a slab.
    Carcass& drawers = *k.baseRow().cabinets()[2];
    drawers.setWorktopJoint(true);
    const std::vector<Worktop*> pieces = k.backRun().worktops();
    REQUIRE(pieces.size() == 2);
    CHECK_THAT(pieces[0]->box(), IsBox({0, 820, 0}, {2400, 858, 600}));
    CHECK_THAT(pieces[1]->box(), IsBox({2400, 820, 0}, {4800, 858, 600}));
    CHECK(k.longWorktops(rules).empty());
    // No edge band at the joint; the sink's opening on the piece it is on.
    CHECK(pieces[0]->panel()->edge(EdgeSide::W1).present());
    CHECK_FALSE(pieces[0]->panel()->edge(EdgeSide::W2).present());
    CHECK_FALSE(pieces[1]->panel()->edge(EdgeSide::W1).present());
    CHECK(pieces[0]->panel()->cutouts().size() == 1);
    CHECK(pieces[1]->panel()->cutouts().empty());
    CHECK(k.misplacedCutouts(rules).empty());
    requireNoOverlaps(*product);

    // A joint right next to the sink leaves too little worktop around its cut-out.
    Carcass& sink = *k.baseRow().cabinets()[1];
    sink.setWorktopJoint(true);
    REQUIRE(k.backRun().worktops().size() == 3);
    REQUIRE(k.misplacedCutouts(rules).size() == 1);
    CHECK(k.misplacedCutouts(rules).front() == &sink);
    sink.setWorktopJoint(false);

    // The last cabinet has no joint to give: the worktop ends there anyway.
    k.baseRow().cabinets().back()->setWorktopJoint(true);
    CHECK(k.backRun().worktops().size() == 2);

    // A new piece takes the first one's choices; a piece that goes keeps nothing behind.
    drawers.setWorktopJoint(false);
    REQUIRE(k.backRun().worktops().size() == 1);
    k.worktop().setInCuttingPlan(true);
    drawers.setWorktopJoint(true);
    CHECK(k.backRun().worktops()[1]->inCuttingPlan());
}

TEST_CASE("A joint before the corner: the back worktop in two pieces", "[core][kitchen][worktop][corner]")
{
    const auto product = createProduct(CatalogItem::KitchenL);
    Kitchen& k = *product->kitchen();
    // The last cabinet of the back row: the worktop goes on over the corner cabinet.
    k.baseRow().cabinets().back()->setWorktopJoint(true);
    const std::vector<Worktop*> pieces = k.backRun().worktops();
    REQUIRE(pieces.size() == 2);
    CHECK(pieces[0]->box().max().x == Approx(1900.0));
    CHECK_THAT(pieces[1]->box(), IsBox({1900, 820, 0}, {3000, 858, 600}));
    CHECK_FALSE(pieces[1]->panel()->edge(EdgeSide::W2).present()); // at the right wall
    requireNoOverlaps(*product);
    // The side run's last cabinet ends under the back worktop: no joint there.
    k.sideRun(Side::Right)->baseRow().cabinets().back()->setWorktopJoint(true);
    CHECK(k.sideRun(Side::Right)->worktops().size() == 1);
}
