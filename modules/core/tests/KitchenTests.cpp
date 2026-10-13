// Kitchen stage: several carcasses in one product — base cabinets on plinths with rails under a
// worktop, wall cabinets on the top line.
//
// Reference kitchen: the catalogue's "Mutfak" — 2400 x 2120 x 560; base cabinets 720 high on a
// 100 mm plinth (setback 40), 38 mm worktop 600 deep, wall cabinets 720 x 320; 18 mm chipboard,
// 3 mm HDF backs in a groove, 100 mm rails. Base row, left to right: doors 400, sink 600,
// drawers 400, hob 600, doors (automatic, 400).

#include "readycad/test/TestHelpers.hpp"

#include "readycad/core/builders/Catalog.hpp"
#include "readycad/core/builders/KitchenBuilder.hpp"
#include "readycad/core/model/Product.hpp"
#include "readycad/core/model/Queries.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

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

Carcass& cabinet(const Product& product, RowLevel level, std::size_t index)
{
    const Kitchen& k = *product.kitchen();
    return *(level == RowLevel::Base ? k.baseRow() : k.wallRow()).cabinets().at(index);
}

std::vector<Real> widths(const CabinetRow& row)
{
    std::vector<Real> result;
    for (const Carcass* c : row.cabinets()) {
        result.push_back(c->box().size().x);
    }
    return result;
}

} // namespace

TEST_CASE("A kitchen run: plinths, base cabinets, worktop, wall cabinets on the top line",
          "[core][kitchen]")
{
    const auto product = createProduct(CatalogItem::Kitchen);
    const Kitchen& k = *product->kitchen();
    REQUIRE(product->carcass() == nullptr);
    CHECK(widths(k.baseRow()) == std::vector<Real>{400, 600, 400, 600, 400});
    CHECK(widths(k.wallRow()) == std::vector<Real>{400, 600, 400, 600, 400});

    // The first base cabinet stands on its plinth: the carcass box includes the plinth zone.
    const Carcass& first = cabinet(*product, RowLevel::Base, 0);
    CHECK_THAT(first.box(), IsBox({0, 0, 0}, {400, 820, 560}));
    REQUIRE(first.plinth() != nullptr);
    CHECK_THAT(first.plinth()->box(), IsBox({0, 0, 502}, {400, 100, 520})); // 40 behind the front
    // The sides stand on the bottom (it carries the legs) and go up to the worktop.
    CHECK_THAT(first.bottom().box(), IsBox({0, 100, 0}, {400, 118, 560}));
    CHECK_THAT(first.leftSide().box(), IsBox({0, 118, 0}, {18, 820, 560}));
    // Flat rails between the sides: the front one flush with the front edge.
    REQUIRE(first.frontRail() != nullptr);
    CHECK_THAT(first.frontRail()->box(), IsBox({18, 802, 460}, {382, 820, 560}));
    CHECK_THAT(first.rearRail()->box(), IsBox({18, 802, 0}, {382, 820, 100}));
    // The back goes into the grooves of the sides, the bottom and the rear rail.
    CHECK_THAT(first.back().box(), IsBox({11.5, 111.5, 16}, {388.5, 808.5, 19}));
    CHECK_THAT(first.interiorBox(), IsBox({18, 118, 19}, {382, 802, 560}));
    // The door covers the carcass, not the plinth.
    const Door& door = *first.interior().doors().at(0);
    CHECK_THAT(door.box(), IsBox({1.5, 101.5, 560}, {398.5, 818.5, 578}));
    CHECK(door.handle()->anchor == HandleAnchor::Top);

    // The worktop lies on the base cabinets.
    CHECK_THAT(k.worktop().box(), IsBox({0, 820, 0}, {2400, 858, 600}));
    CHECK(k.worktop().panel()->thickness() == Approx(38.0));
    CHECK_FALSE(k.worktop().inCuttingPlan()); // bought as a slab and cut on its own

    // Wall cabinets hang from the top line; their doors stand in front of THEIR carcass.
    const Carcass& wall = cabinet(*product, RowLevel::Wall, 1);
    CHECK_THAT(wall.box(), IsBox({400, 1400, 0}, {1000, 2120, 320}));
    CHECK(wall.plinth() == nullptr);
    CHECK_THAT(wall.bottom().box(), IsBox({400, 1400, 0}, {1000, 1418, 320})); // under the sides
    CHECK_THAT(wall.top().box(), IsBox({418, 2102, 0}, {982, 2120, 320}));     // between them
    CHECK(wall.interior().doors().at(0)->box().min().z == Approx(320.0));
    CHECK(wall.interior().doors().at(0)->handle()->anchor == HandleAnchor::Bottom);

    requireNoOverlaps(*product);
}

TEST_CASE("Upright rails leave room for a sink: front rail on edge, rear one before the back",
          "[core][kitchen]")
{
    const auto product = createProduct(CatalogItem::Kitchen);
    const Carcass& sink = cabinet(*product, RowLevel::Base, 1);
    REQUIRE(sink.topStyle() == CarcassTop::UprightRails);
    CHECK_THAT(sink.frontRail()->box(), IsBox({418, 720, 542}, {982, 820, 560}));
    CHECK_THAT(sink.rearRail()->box(), IsBox({418, 720, 19}, {982, 820, 37}));
    // The back reaches up to the top (no groove above it), between the side and bottom grooves.
    CHECK_THAT(sink.back().box(), IsBox({411.5, 111.5, 16}, {988.5, 820, 19}));
    // The opening ends under the rails; the doors still cover the carcass up to the top.
    CHECK(sink.interiorBox().max().y == Approx(720.0));
    CHECK(sink.interior().doors().at(0)->box().max().y == Approx(818.5));
    CHECK(sink.interior().shelves().empty()); // the siphon needs the room
}

TEST_CASE("Rails fit every back mount without overlapping", "[core][kitchen]")
{
    const BackMountType mount =
        GENERATE(BackMountType::Groove, BackMountType::Overlay, BackMountType::Inset);
    const CarcassTop top = GENERATE(CarcassTop::FlatRails, CarcassTop::UprightRails);
    CAPTURE(static_cast<int>(mount), static_cast<int>(top));

    auto product = std::make_shared<Product>();
    auto carcass = std::make_shared<Carcass>();
    carcass->setBackMount(mount);
    carcass->setTopStyle(top);
    carcass->setBottomJoint(JointType::Overlay);
    carcass->setPlinth(true);
    carcass->interior().setAutoShelfCount(1);
    carcass->interior().setDoors(2);
    product->add(carcass);
    product->setDimensions({600, 820, 560});
    requireNoOverlaps(*product);

    const Aabb& back = carcass->back().box();
    // Upright rear rail: right in front of the back. Flat: on the rear edges, over the back.
    if (top == CarcassTop::UprightRails) {
        CHECK(carcass->rearRail()->box().min().z == Approx(back.max().z));
    } else {
        CHECK(carcass->rearRail()->box().min().z == Approx(carcass->leftSide().box().min().z));
    }
    // Switching back to a top panel brings the panel back, without rails.
    carcass->setTopStyle(CarcassTop::Panel);
    CHECK(carcass->frontRail() == nullptr);
    CHECK(carcass->top().parent() == carcass.get());
    requireNoOverlaps(*product);
}

TEST_CASE("Rails keep the user's material when they turn upright", "[core][kitchen]")
{
    auto product = std::make_shared<Product>();
    auto carcass = std::make_shared<Carcass>();
    product->add(carcass);
    carcass->setTopStyle(CarcassTop::FlatRails);
    const Material oak{"Meşe 18", MaterialType::Chipboard, 18.0, true};
    carcass->frontRail()->setMaterialOverride(oak);
    carcass->setTopStyle(CarcassTop::UprightRails);
    REQUIRE(carcass->frontRail()->upright());
    CHECK(carcass->frontRail()->materialOverride() == oak);
    // The rails take the top panel's place in the tree: the tree reads the same way.
    CHECK(carcass->children()[2].get() == carcass->frontRail());
    CHECK(carcass->children()[3].get() == carcass->rearRail());
}

TEST_CASE("Resizing the run: the automatic cabinet takes the change", "[core][kitchen]")
{
    const auto product = createProduct(CatalogItem::Kitchen);
    product->setDimensions({2600, 2220, 580});
    const Kitchen& k = *product->kitchen();
    CHECK(widths(k.baseRow()) == std::vector<Real>{400, 600, 400, 600, 600});
    CHECK(widths(k.wallRow()) == std::vector<Real>{400, 600, 400, 600, 600});
    CHECK(k.worktop().box().size().x == Approx(2600.0));
    CHECK(cabinet(*product, RowLevel::Wall, 0).box().min().y == Approx(1500.0)); // 2220 - 720
    CHECK(cabinet(*product, RowLevel::Base, 0).box().max().z == Approx(580.0));
    requireNoOverlaps(*product);

    // The kitchen's own sizes: taller base cabinets lift the worktop.
    Kitchen::Sizes sizes = k.sizes();
    sizes.baseHeight = 760.0;
    product->kitchen()->setSizes(sizes);
    CHECK(k.worktop().box().min().y == Approx(860.0)); // 100 + 760
    CHECK_THROWS_AS(product->kitchen()->setSizes({0, 720, 320, 600}), std::invalid_argument);
}

TEST_CASE("Worktop cut-outs: over their cabinet, 60 mm behind the front edge", "[core][kitchen]")
{
    const auto product = createProduct(CatalogItem::Kitchen);
    const Kitchen& k = *product->kitchen();
    const auto& cutouts = k.worktop().panel()->cutouts();
    REQUIRE(cutouts.size() == 2);
    // Panel coordinates: x along the run from the worktop's left end, y from the wall.
    CHECK(cutouts[0].center.x == Approx(700.0)); // the sink cabinet: 400...1000
    CHECK(cutouts[0].center.y == Approx(300.0)); // 600 - 60 - 480 / 2
    CHECK(cutouts[0].length == Approx(560.0));
    CHECK(cutouts[0].width == Approx(480.0));
    CHECK(cutouts[1].center.x == Approx(1700.0)); // the hob cabinet: 1400...2000
    CHECK(cutouts[1].center.y == Approx(295.0));  // 600 - 60 - 490 / 2
    CHECK(k.misplacedCutouts(product->rules()).empty());

    // Cut-outs are part of the panel's shape: detailing's clean-up keeps them.
    k.worktop().panel()->clearMachining();
    CHECK(k.worktop().panel()->cutouts().size() == 2);
}

TEST_CASE("A cut-out must fall into its cabinet and leave worktop around it", "[core][kitchen]")
{
    const auto product = createProduct(CatalogItem::Kitchen);
    Kitchen& k = *product->kitchen();
    const ConstructionRules& rules = product->rules();
    Carcass& sink = cabinet(*product, RowLevel::Base, 1);
    REQUIRE(k.misplacedCutouts(rules).empty());

    SECTION("flat rails under a sink: the bowl would hit the front rail")
    {
        sink.setTopStyle(CarcassTop::FlatRails);
        CHECK(k.misplacedCutouts(rules) == std::vector<const Carcass*>{&sink});
    }
    SECTION("a 560 mm sink in a 400 mm cabinet: it would hit the side walls")
    {
        sink.setFixedWidth(400.0);
        CHECK(k.misplacedCutouts(rules) == std::vector<const Carcass*>{&sink});
    }
    SECTION("at the end of the run only 20 mm of worktop would be left")
    {
        k.baseRow().moveCabinet(sink.id(), -1);
        CHECK(&cabinet(*product, RowLevel::Base, 0) == &sink);
        CHECK(k.misplacedCutouts(rules) == std::vector<const Carcass*>{&sink});
    }
    SECTION("too close to the front edge")
    {
        WorktopCutout c = *sink.cutout();
        c.frontOffset = 20.0;
        sink.setCutout(c);
        CHECK(k.misplacedCutouts(rules) == std::vector<const Carcass*>{&sink});
    }
    SECTION("a valid smaller sink")
    {
        sink.setCutout(WorktopCutout{CutoutKind::Sink, 400.0, 400.0, 10.0, 80.0});
        CHECK(k.misplacedCutouts(rules).empty());
    }
    CHECK_THROWS_AS(sink.setCutout(WorktopCutout{CutoutKind::Sink, 100.0, 400.0, 60.0, 60.0}),
                    std::invalid_argument); // corners rounder than the cut-out
}

TEST_CASE("Moving a cabinet: the cabinets keep their widths, the positions follow",
          "[core][kitchen]")
{
    const auto product = createProduct(CatalogItem::Kitchen);
    CabinetRow& row = product->kitchen()->baseRow();
    Carcass& drawers = *row.cabinets()[2];
    row.moveCabinet(drawers.id(), 10); // clamped: to the right end
    CHECK(row.cabinets().back() == &drawers);
    CHECK(widths(row) == std::vector<Real>{400, 600, 600, 400, 400});
    CHECK(drawers.box().max().x == Approx(2400.0));
    CHECK_THROWS_AS(row.moveCabinet(product->id(), 1), std::invalid_argument);
    requireNoOverlaps(*product);
}

TEST_CASE("A plinth lifts any carcass; its box keeps the size", "[core][kitchen]")
{
    auto product = std::make_shared<Product>();
    auto carcass = std::make_shared<Carcass>();
    product->add(carcass);
    product->setDimensions({800, 2100, 560});
    carcass->setPlinth(true);
    CHECK_THAT(carcass->box(), IsBox({0, 0, 0}, {800, 2100, 560}));
    CHECK(carcass->leftSide().box().min().y == Approx(100.0)); // inset bottom: walls to the plinth
    CHECK(carcass->leftSide().box().size().y == Approx(1982.0)); // 2000 - 18 under the top
    CHECK_THAT(carcass->plinth()->box(), IsBox({0, 0, 502}, {800, 100, 520}));
    carcass->setPlinth(false);
    CHECK(carcass->plinth() == nullptr);
    CHECK(carcass->leftSide().box().min().y == Approx(0.0));
}

TEST_CASE("Kitchen builder: one door up to 500 mm, two above; a worktop needs base cabinets",
          "[core][kitchen]")
{
    const auto product = KitchenBuilder()
                             .length(1800)
                             .add(CabinetModule::BaseDoors, 450)
                             .add(CabinetModule::BaseDoors)
                             .add(CabinetModule::WallOpen)
                             .build();
    const Kitchen& k = *product->kitchen();
    CHECK(k.baseRow().cabinets()[0]->interior().doors().size() == 1);
    CHECK(k.baseRow().cabinets()[1]->interior().doors().size() == 2); // 1350 wide
    CHECK(k.wallRow().cabinets()[0]->interior().doors().empty());
    CHECK(k.wallRow().cabinets()[0]->box().size().x == Approx(1800.0));
    requireNoOverlaps(*product);

    CHECK_THROWS_AS(KitchenBuilder().add(CabinetModule::WallDoors).build(), std::invalid_argument);
    CHECK_THROWS_AS(KitchenBuilder().add(CabinetModule::Sink, 0.0).build(), std::invalid_argument);
}

TEST_CASE("A product holds one carcass or one kitchen", "[core][kitchen]")
{
    const auto product = createProduct(CatalogItem::Kitchen);
    CHECK_THROWS_AS(product->add(std::make_shared<Carcass>()), std::invalid_argument);
    CHECK_THROWS_AS(product->kitchen()->baseRow().add(std::make_shared<Section>()),
                    std::invalid_argument);
}
