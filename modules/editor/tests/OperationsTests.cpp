// Stage 5: what the parameter panel sends really arrives in the model (and in the drilling).

#include "readycad/editor/Operations.hpp"
#include "readycad/editor/PartInfo.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

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

    setBackMount(doc, BackMountType::Overlay);
    CHECK(doc.product().carcass()->backMountType() == BackMountType::Overlay);
    CHECK(doc.detailingReport().grooves == 0);

    setTopJoint(doc, JointType::Inset);
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
