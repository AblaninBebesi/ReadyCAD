// Stage 5: what the parameter panel sends really arrives in the model (and in the drilling).

#include "readycad/editor/Operations.hpp"
#include "readycad/editor/PartInfo.hpp"
#include "readycad/i18n/Localization.hpp"
#include "readycad/storage/Storage.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <functional>
#include <string>

using namespace rc::core;
using namespace rc::editor;
using Catch::Approx;

namespace {

int holesOfType(const FurnitureComponent& part, FastenerType type)
{
    int count = 0;
    for (const Drilling& d : part.panel()->drillings()) {
        count += d.fastener == type ? 1 : 0;
    }
    return count;
}

} // namespace

TEST_CASE("Product parameters", "[editor][operations]")
{
    Document doc = Document::fromCatalog(CatalogItem::Wardrobe);

    setDimensions(doc, {900, 2100, 580});
    CHECK(doc.product().dimensions() == Size3D{900, 2100, 580});
    CHECK(doc.product().carcass()->rightSide().box().max().x == Approx(900.0));

    setProductName(doc, "Çocuk odası dolabı");
    CHECK(doc.product().name() == "Çocuk odası dolabı");

    const ComponentId carcass = doc.product().carcass()->id();
    setBackMount(doc, carcass, BackMountType::Overlay);
    CHECK(doc.product().carcass()->backMountType() == BackMountType::Overlay);
    CHECK(doc.detailingReport().grooves == 0);

    setTopJoint(doc, carcass, JointType::Inset);
    CHECK(doc.product().carcass()->leftSide().size().height == Approx(2100.0));
}

TEST_CASE("Section parameters: shelves, doors, drawers, splitting", "[editor][operations]")
{
    Document doc = Document::fromCatalog(CatalogItem::Wardrobe);
    Section& interior = doc.product().carcass()->interior();

    setShelfCount(doc, interior.id(), 2, ShelfMount::Confirmat);
    CHECK(interior.shelves().size() == 2);
    CHECK(interior.shelves()[0]->mount() == ShelfMount::Confirmat);
    CHECK(doc.detailingReport().joints == 4 + 2 * 2);

    setDoorCount(doc, interior.id(), 1);
    CHECK(interior.doors().size() == 1);

    clearSection(doc, interior.id());
    CHECK(interior.children().empty());

    splitSection(doc, interior.id(), Axis::X, 3);
    const auto columns = interior.subsections();
    REQUIRE(columns.size() == 3);

    setSectionFixedSize(doc, columns[0]->id(), 300.0);
    CHECK(columns[0]->size().width == Approx(300.0));

    setDrawerCount(doc, columns[2]->id(), 3);
    REQUIRE(columns[2]->drawerStack() != nullptr);
    CHECK(columns[2]->drawerStack()->count() == 3);
    CHECK(doc.detailingReport().warnings.empty());
}

TEST_CASE("Part parameters: shelf, door, handle, material, removal", "[editor][operations]")
{
    Document doc = Document::fromCatalog(CatalogItem::Wardrobe);
    Section& interior = doc.product().carcass()->interior();
    Shelf& shelf = *interior.shelves()[0];
    Door& door = *interior.doors()[0];

    setShelfMount(doc, shelf.id(), ShelfMount::Confirmat);
    CHECK(shelf.mount() == ShelfMount::Confirmat);
    CHECK(shelf.size().width == Approx(764.0)); // tight between the walls now

    setShelfElevation(doc, shelf.id(), 300.0);
    CHECK(shelf.box().min().y == Approx(318.0)); // confirmat shelves are not snapped

    setHinge(doc, door.id(), HingeSide::Right);
    CHECK(door.hinge() == HingeSide::Right);

    setHandle(doc, door.id(), HandlePlacement{"knob-30"});
    CHECK(holesOfType(door, FastenerType::Handle) == 1);
    setHandle(doc, door.id(), std::nullopt);
    CHECK(holesOfType(door, FastenerType::Handle) == 0);

    setMaterialOverride(doc, door.id(), Material{"MDF 22", MaterialType::MDF, 22.0, false});
    CHECK(door.size().depth == Approx(22.0));

    const auto shelves = interior.shelves().size();
    removeComponent(doc, interior.shelves().back()->id());
    CHECK(interior.shelves().size() == shelves - 1);
}

TEST_CASE("A cut list row is left out of the cutting plan in one undoable step",
          "[editor][operations]")
{
    Document doc = Document::fromCatalog(CatalogItem::Wardrobe);
    const auto shelfRow = [&doc]() -> const rc::detailing::CutListRow* {
        for (const auto& row : doc.cutList()) {
            if (row.kind == ComponentKind::Shelf) {
                return &row;
            }
        }
        return nullptr;
    };
    REQUIRE(shelfRow() != nullptr);
    const std::vector<ComponentId> shelves = shelfRow()->components;
    REQUIRE(shelves.size() == 4);

    setInCuttingPlan(doc, shelves, false);
    REQUIRE(shelfRow() != nullptr);
    CHECK_FALSE(shelfRow()->inCuttingPlan);
    CHECK(shelfRow()->quantity == 4); // still one row: all of them changed together
    CHECK(doc.undoDescription() == "Cutting plan");

    doc.undo();
    CHECK(shelfRow()->inCuttingPlan);

    // A wrong id changes nothing, not even the parts before it in the list.
    CHECK_THROWS_AS(setInCuttingPlan(doc, {shelfRow()->components[0], ComponentId{999999}}, false),
                    std::invalid_argument);
    CHECK(shelfRow()->inCuttingPlan);
    CHECK(shelfRow()->quantity == 4);
}

TEST_CASE("An edit that would make the product impossible to build is refused",
          "[editor][operations][overlap]")
{
    // Stage 9: the random-edit test found these ways to put parts into each other's space.
    Document doc = Document::fromCatalog(CatalogItem::WardrobeWithDrawers);
    const auto column = [&doc] { return doc.product().carcass()->interior().subsections()[0]; };
    const auto refused = [&](const std::function<void()>& edit) {
        const std::string before = rc::storage::saveProduct(doc.product());
        CHECK_THROWS_AS(edit(), ConstructionError);
        CHECK(rc::storage::saveProduct(doc.product()) == before); // nothing changed
        CHECK(doc.overlaps().empty());
    };

    // Doors over the whole column would cover the drawer fronts of its lower compartment.
    refused([&] { setDoorCount(doc, column()->id(), 2); });
    // Five shelves do not fit into a 100 mm compartment.
    setDrawerCount(doc, column()->subsections()[0]->id(), 0);
    setSectionFixedSize(doc, column()->subsections()[0]->id(), 100.0);
    refused([&] { setShelfCount(doc, column()->subsections()[0]->id(), 5); });
    // Drawers would get boxes without height there.
    refused([&] { setDrawerCount(doc, column()->subsections()[0]->id(), 3); });

    // A refused edit tells why, in the language of the interface.
    try {
        setDoorCount(doc, column()->id(), 1);
        FAIL("the doors were accepted");
    } catch (const ConstructionError& e) {
        CHECK_THAT(e.what(), Catch::Matchers::ContainsSubstring(
                                 std::string(rc::i18n::text(rc::i18n::TextId::PartsOverlap))));
    }

    // A thicker top is fine now: its neighbours follow it.
    setMaterialOverride(doc, doc.product().carcass()->top().id(),
                        Material{"Suntalam 25", MaterialType::Chipboard, 25.0, true});
    CHECK(doc.overlaps().empty());
}

TEST_CASE("Kitchen cabinets: add, size, move, remove, each one undoable step",
          "[editor][operations][kitchen]")
{
    Document doc = Document::fromCatalog(CatalogItem::Kitchen);
    const auto base = [&doc] { return doc.product().kitchen()->baseRow().cabinets(); };
    REQUIRE(base().size() == 5);

    // No room: the automatic cabinet (400) would lose its whole width.
    CHECK_THROWS_AS(addCabinet(doc, CabinetModule::BaseDoors, 600.0), ConstructionError);
    CHECK(base().size() == 5);

    setDimensions(doc, {3000, 2120, 560});
    addCabinet(doc, CabinetModule::BaseDrawers, 600.0);
    REQUIRE(base().size() == 6);
    CHECK(base().back()->interior().drawerStack() != nullptr);
    CHECK(base().back()->box().max().x == Approx(3000.0));

    const ComponentId added = base().back()->id();
    setCabinetWidth(doc, added, 500.0);
    CHECK(base().back()->box().size().x == Approx(500.0));
    CHECK(base()[4]->box().size().x == Approx(500.0)); // the automatic one took the 100 mm

    moveCabinet(doc, added, -10);
    CHECK(base().front()->id() == added);
    CHECK(base().front()->box().min().x == Approx(0.0));

    removeComponent(doc, added);
    CHECK(base().size() == 5);
    doc.undo();
    CHECK(base().size() == 6);
    CHECK(doc.undoDescription() == "Move cabinet");

    // Kitchen operations need a kitchen.
    Document wardrobe = Document::fromCatalog(CatalogItem::Wardrobe);
    CHECK_THROWS_AS(addCabinet(wardrobe, CabinetModule::WallDoors, 600.0), std::invalid_argument);
    CHECK_THROWS_AS(setCabinetWidth(wardrobe, wardrobe.product().carcass()->id(), 500.0),
                    std::invalid_argument);
}

TEST_CASE("Kitchen edits that cannot be built are refused", "[editor][operations][kitchen]")
{
    Document doc = Document::fromCatalog(CatalogItem::Kitchen);
    const auto refused = [&](const std::function<void()>& edit, rc::i18n::TextId reason) {
        const std::string before = rc::storage::saveProduct(doc.product());
        try {
            edit();
            FAIL("the edit was accepted");
        } catch (const ConstructionError& e) {
            CHECK_THAT(e.what(), Catch::Matchers::ContainsSubstring(
                                     std::string(rc::i18n::text(reason))));
        }
        CHECK(rc::storage::saveProduct(doc.product()) == before);
    };
    const auto sink = [&doc] { return doc.product().kitchen()->baseRow().cabinets()[1]; };

    // Flat rails under the sink: the bowl would hit the front rail.
    refused([&] { setTopStyle(doc, sink()->id(), CarcassTop::FlatRails); },
            rc::i18n::TextId::CutoutDoesNotFit);
    // Without the sink the cabinet may have them.
    setCutout(doc, sink()->id(), std::nullopt);
    setTopStyle(doc, sink()->id(), CarcassTop::FlatRails);
    CHECK(doc.product().kitchen()->worktop().panel()->cutouts().size() == 1);

    // Wall cabinets down to the worktop.
    Kitchen::Sizes sizes = doc.product().kitchen()->sizes();
    sizes.wallHeight = 1300.0;
    refused([&] { setKitchenSizes(doc, sizes); }, rc::i18n::TextId::PartsOverlap);

    // The last base cabinet carries the worktop.
    while (doc.product().kitchen()->baseRow().cabinets().size() > 1) {
        removeComponent(doc, doc.product().kitchen()->baseRow().cabinets().back()->id());
    }
    refused([&] { removeComponent(doc, doc.product().kitchen()->baseRow().cabinets()[0]->id()); },
            rc::i18n::TextId::PartWithoutSize);
}

TEST_CASE("Carcass construction is set per carcass", "[editor][operations][kitchen]")
{
    Document doc = Document::fromCatalog(CatalogItem::Kitchen);
    Carcass& wall = *doc.product().kitchen()->wallRow().cabinets()[0];
    setBackMount(doc, wall.id(), BackMountType::Overlay);
    setPlinth(doc, doc.product().kitchen()->wallRow().cabinets()[1]->id(), false);
    const Kitchen& k = *doc.product().kitchen();
    CHECK(k.wallRow().cabinets()[0]->backMountType() == BackMountType::Overlay);
    CHECK(k.wallRow().cabinets()[1]->backMountType() == BackMountType::Groove);
    CHECK(k.baseRow().cabinets()[0]->hasPlinth());
}

TEST_CASE("Part info for the properties panel", "[editor][partinfo]")
{
    const Document doc = Document::fromCatalog(CatalogItem::Wardrobe);
    const SideWall& side = doc.product().carcass()->leftSide();

    const PartInfo info = describe(side, doc.product().rules());
    CHECK(info.kind == ComponentKind::SideWall);
    CHECK(info.size == Size3D{18, 1982, 560});
    REQUIRE(info.panel.has_value());
    CHECK(info.panel->finishLength == Approx(1982.0));
    CHECK(info.panel->cut.width == Approx(559.0));
    CHECK(info.panel->edges[static_cast<std::size_t>(EdgeSide::L2)].thickness == 2.0);
    CHECK(info.panel->holes.at(FastenerType::ShelfSupport) == 8);
    CHECK(info.panel->grooveCount == 1);

    CHECK_FALSE(describe(doc.product(), doc.product().rules()).panel.has_value());
}

TEST_CASE("A part's shape: accepted when it can be made, refused otherwise", "[editor][operations][shape]")
{
    Document doc = Document::fromCatalog(CatalogItem::Wardrobe);
    const auto wall = [&doc] { return &doc.product().carcass()->leftSide(); };
    const auto refused = [&](std::vector<ShapeFeature> shape, rc::i18n::TextId reason) {
        const std::string before = rc::storage::saveProduct(doc.product());
        try {
            setShape(doc, wall()->id(), std::move(shape));
            FAIL("the shape was accepted");
        } catch (const ConstructionError& e) {
            CHECK_THAT(e.what(), Catch::Matchers::ContainsSubstring(std::string(rc::i18n::text(reason))));
            CHECK_THAT(e.what(), Catch::Matchers::ContainsSubstring("Yan panel"));
        }
        CHECK(rc::storage::saveProduct(doc.product()) == before);
    };

    // The skirting board notch and a cable hole.
    setShape(doc, wall()->id(), {CornerNotch{PanelCorner::L1W1, 100.0, 15.0},
                                 RoundHole{PanelCorner::L1W1, 900.0, 120.0, 60.0}});
    CHECK(wall()->shape().size() == 2);
    CHECK(doc.shapeConflicts().empty());
    CHECK(doc.undoDescription() == "Part shape");

    // Deeper than the back panel groove; two features in one corner.
    refused({CornerNotch{PanelCorner::L1W1, 100.0, 20.0}}, rc::i18n::TextId::MachiningInCut);
    refused({CornerRound{PanelCorner::L2W2, 30.0}, CornerNotch{PanelCorner::L2W2}},
            rc::i18n::TextId::ShapeDoesNotFit);
    // An impossible size never reaches the model.
    CHECK_THROWS_AS(setShape(doc, wall()->id(), {RoundHole{PanelCorner::L1W1, 900.0, 120.0, -5.0}}),
                    std::invalid_argument);

    // The shape follows the wall; a lower wardrobe cannot drop its hole off the part.
    setDimensions(doc, {800, 2200, 560});
    CHECK(wall()->panel()->cutouts().size() == 1);
    doc.undo();
    doc.undo();
    CHECK(wall()->shape().empty());
}

TEST_CASE("Corner kitchens: side walls, corner cabinets, undoable", "[editor][operations][corner]")
{
    Document doc = Document::fromCatalog(CatalogItem::Kitchen);
    const auto kitchen = [&doc] { return doc.product().kitchen(); };
    setDimensions(doc, {3400, 2120, 560});

    // A right wall: an L kitchen, with its corner and one cabinet of each row.
    setSideRun(doc, Side::Right, 2000.0);
    REQUIRE(kitchen()->runs().size() == 2);
    const KitchenRun* right = kitchen()->sideRun(Side::Right);
    CHECK(right->length() == Approx(2000.0));
    CHECK(right->baseRow().cabinets().size() == 1);
    CHECK(right->wallRow().cabinets().size() == 1);
    REQUIRE(kitchen()->backRun().corner(Side::Right) != nullptr);

    // Cabinets go to the wall asked for.
    addCabinet(doc, CabinetModule::BaseDrawers, 400.0, KitchenWall::Right);
    CHECK(kitchen()->sideRun(Side::Right)->baseRow().cabinets().size() == 2);
    CHECK(kitchen()->baseRow().cabinets().size() == 5);
    CHECK_THROWS_AS(addCabinet(doc, CabinetModule::BaseDoors, 400.0, KitchenWall::Left),
                    std::invalid_argument);

    // A corner cabinet: its width and filler change, it does not move, leave, or go automatic.
    const Carcass& base = kitchen()->backRun().corner(Side::Right)->baseCabinet();
    const ComponentId id = base.id();
    const Real door = base.interior().doors().at(0)->box().size().x;
    setBlindFiller(doc, id, 80.0);
    CHECK(kitchen()->backRun().corner(Side::Right)->baseCabinet().interior().doors().at(0)->box().size().x
          == Approx(door - 30.0));
    setCabinetWidth(doc, id, 1200.0);
    CHECK(kitchen()->backRun().corner(Side::Right)->baseCabinet().box().size().x == Approx(1200.0));
    CHECK_THROWS_AS(setCabinetWidth(doc, id, std::nullopt), std::invalid_argument);
    CHECK_THROWS_AS(moveCabinet(doc, id, -1), std::invalid_argument);
    CHECK_THROWS_AS(removeComponent(doc, id), std::logic_error);
    CHECK_THROWS_AS(setCutout(doc, id, WorktopCutout::standard(CutoutKind::Sink)), std::invalid_argument);
    CHECK_THROWS_AS(setBlindFiller(doc, kitchen()->baseRow().cabinets()[0]->id(), 50.0),
                    std::invalid_argument);

    // Both walls: a U kitchen — the back wall needs room for a second corner cabinet first.
    CHECK_THROWS_AS(setSideRun(doc, Side::Left, 1800.0), ConstructionError);
    setDimensions(doc, {4800, 2120, 560});
    setSideRun(doc, Side::Left, 1800.0);
    CHECK(kitchen()->runs().size() == 3);
    setSideRun(doc, Side::Right, 2400.0);
    CHECK(kitchen()->sideRun(Side::Right)->baseRow().cabinets().size() == 2);
    CHECK(kitchen()->sideRun(Side::Right)->length() == Approx(2400.0));

    // Taken away again: the run, its cabinets and its corner; one undo brings them back.
    setSideRun(doc, Side::Right, std::nullopt);
    CHECK(kitchen()->sideRun(Side::Right) == nullptr);
    CHECK(kitchen()->backRun().corner(Side::Right) == nullptr);
    doc.undo();
    REQUIRE(kitchen()->sideRun(Side::Right) != nullptr);
    CHECK(kitchen()->sideRun(Side::Right)->baseRow().cabinets().size() == 2);
    CHECK(kitchen()->backRun().corner(Side::Right)->baseCabinet().blindFiller() == Approx(80.0));

    // A side wall too short for its cabinets is refused.
    const std::string before = rc::storage::saveProduct(doc.product());
    CHECK_THROWS_AS(setSideRun(doc, Side::Left, 500.0), ConstructionError); // shorter than the corner
    CHECK(rc::storage::saveProduct(doc.product()) == before);
}

TEST_CASE("Worktop joints: on base cabinets, never through a cut-out", "[editor][operations][worktop]")
{
    Document doc = Document::fromCatalog(CatalogItem::Kitchen);
    setDimensions(doc, {4600, 2120, 560});
    const auto kitchen = [&doc] { return doc.product().kitchen(); };
    REQUIRE(doc.longWorktops().size() == 1); // 4600 from one slab: warned, not refused

    // Doors 400 | sink 600 | drawers 400 | hob 600 | doors 2000 | doors 600: next to the sink
    // and the hob a joint would leave too little worktop around their cut-outs; after the long
    // doors it is clear of both.
    addCabinet(doc, CabinetModule::BaseDoors, 600.0);
    setWorktopJoint(doc, kitchen()->baseRow().cabinets()[4]->id(), true);
    REQUIRE(kitchen()->backRun().worktops().size() == 2);
    CHECK(kitchen()->backRun().worktops()[0]->box().size().x == Approx(4000.0));
    CHECK(doc.longWorktops().empty());
    CHECK(doc.undoDescription() == "Worktop joint");

    // Right after the sink: no worktop left around its cut-out.
    const std::string before = rc::storage::saveProduct(doc.product());
    CHECK_THROWS_AS(setWorktopJoint(doc, kitchen()->baseRow().cabinets()[1]->id(), true),
                    ConstructionError);
    CHECK(rc::storage::saveProduct(doc.product()) == before);
    // Wall cabinets carry no worktop.
    CHECK_THROWS_AS(setWorktopJoint(doc, kitchen()->wallRow().cabinets()[0]->id(), true),
                    std::invalid_argument);

    doc.undo();
    CHECK(kitchen()->backRun().worktops().size() == 1);
}
