// Project and workshop-profile files: everything the user designed survives save + load.

#include "readycad/test/TestHelpers.hpp"

#include "readycad/core/builders/Catalog.hpp"
#include "readycad/core/builders/WardrobeBuilder.hpp"
#include "readycad/core/model/Drawer.hpp"
#include "readycad/core/model/Queries.hpp"
#include "readycad/storage/Storage.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <filesystem>

using namespace rc::core;
using namespace rc::storage;
using rc::test::IsBox;

namespace {

/// Same geometry: every panel has the same box, in the same tree order.
void requireSameGeometry(const Product& a, const Product& b)
{
    const auto panelsA = collectPanels(a);
    const auto panelsB = collectPanels(b);
    REQUIRE(panelsA.size() == panelsB.size());
    for (std::size_t i = 0; i < panelsA.size(); ++i) {
        CHECK(panelsA[i]->kind() == panelsB[i]->kind());
        CHECK_THAT(panelsB[i]->box(), IsBox(panelsA[i]->box().min(), panelsA[i]->box().max()));
    }
}

} // namespace

TEST_CASE("Workshop profile round trip", "[storage][profile]")
{
    ConstructionRules rules;
    rules.carcassMaterial = {"Suntalam 16 Antrasit", MaterialType::Chipboard, 16.0, true};
    rules.joints.fastener = ConstructionRules::JointFastener::Eccentric;
    rules.handles.drillOnMachine = false;
    rules.handles.catalog.push_back({"tedarikci-g-224", "Gola 224", "Tedarikçi A.Ş.", "G-224",
                                     HandleType::Bar, 224.0, 5.0, 240.0, 14.0, 28.0});
    rules.handles.defaultDoorHandle = "tedarikci-g-224";

    const std::string text = saveProfile(rules);
    const ConstructionRules loaded = loadProfile(text);

    CHECK(saveProfile(loaded) == text);
    CHECK(loaded.carcassMaterial.thickness == 16.0);
    CHECK(loaded.joints.fastener == ConstructionRules::JointFastener::Eccentric);
    CHECK_FALSE(loaded.handles.drillOnMachine);
    REQUIRE(loaded.handles.find("tedarikci-g-224") != nullptr);
    CHECK(loaded.handles.find("tedarikci-g-224")->supplier == "Tedarikçi A.Ş."); // UTF-8 kept
}

TEST_CASE("Missing profile fields keep their defaults", "[storage][profile]")
{
    const ConstructionRules loaded = loadProfile(
        R"({"format": "readycad-profile", "version": 1, "rules": {"frontGap": 4.0}})");
    const ConstructionRules defaults;
    CHECK(loaded.frontGap == 4.0);
    CHECK(loaded.shelfSideGap == defaults.shelfSideGap);
    CHECK(loaded.system32.pitch == 32.0);
    CHECK(loaded.handles.catalog.size() == defaults.handles.catalog.size());
}

TEST_CASE("Every catalogue product survives save and load", "[storage][project]")
{
    const CatalogItem item = GENERATE(from_range(catalogItems().begin(), catalogItems().end()));
    const auto original = createProduct(item);

    const std::string text = saveProduct(*original);
    const auto loaded = loadProduct(text);

    CHECK(saveProduct(*loaded) == text);
    CHECK(loaded->dimensions() == original->dimensions());
    requireSameGeometry(*original, *loaded);
}

TEST_CASE("Per-part choices survive save and load", "[storage][project]")
{
    auto product = WardrobeBuilder()
                       .name("Yatak odası dolabı")
                       .dimensions({1200, 2100, 600})
                       .sections(2)
                       .sectionWidth(0, 450.0)
                       .shelvesPerSection(2)
                       .doorsPerSection(1)
                       .backMount(BackMountType::Overlay)
                       .build();
    Carcass& c = *product->carcass();
    c.leftSide().setMaterialOverride(
        Material{"Suntalam 25", MaterialType::Chipboard, 25.0, true});
    auto columns = c.interior().subsections();
    columns[0]->addShelf(800.0, ShelfMount::Confirmat);
    columns[0]->shelves().front()->setEdgeOverride(EdgeSide::L1, EdgeBand{0.4, "PVC 0.4"});
    columns[1]->doors().front()->setHinge(HingeSide::Right);
    columns[1]->doors().front()->setHandle(
        HandlePlacement{"knob-30", HandleOrientation::Vertical, HandleAnchor::Top, 50.0, 80.0});
    columns[0]->doors().front()->setHandle(std::nullopt);

    const auto loaded = loadProduct(saveProduct(*product));
    requireSameGeometry(*product, *loaded);

    const Carcass& lc = *loaded->carcass();
    CHECK(loaded->name() == "Yatak odası dolabı");
    CHECK(lc.backMountType() == BackMountType::Overlay);
    CHECK(lc.leftSide().material().thickness == 25.0);
    const auto lcolumns = lc.interior().subsections();
    CHECK(lcolumns[0]->fixedSize() == 450.0);

    const auto shelves = lcolumns[0]->shelves();
    REQUIRE(shelves.size() == 3);
    CHECK(shelves[2]->mount() == ShelfMount::Confirmat);
    CHECK(shelves[2]->elevation() == 800.0);
    CHECK(shelves[0]->edgeOverride(EdgeSide::L1)->name == "PVC 0.4");

    CHECK_FALSE(lcolumns[0]->doors().front()->handle().has_value());
    const Door& door = *lcolumns[1]->doors().front();
    CHECK(door.hinge() == HingeSide::Right);
    CHECK(door.handle()->modelId == "knob-30");
    CHECK(door.handle()->anchor == HandleAnchor::Top);
    CHECK(door.handle()->anchorOffset == 80.0);
}

TEST_CASE("Drawer front handles survive save and load", "[storage][project]")
{
    auto chest = createProduct(CatalogItem::ChestOfDrawers);
    auto drawers = chest->carcass()->interior().drawerStack()->drawers();
    drawers[3]->front().setHandle(HandlePlacement{"bar-256", HandleOrientation::Horizontal});

    const auto loaded = loadProduct(saveProduct(*chest));
    const auto loadedDrawers = loaded->carcass()->interior().drawerStack()->drawers();
    REQUIRE(loadedDrawers.size() == 4);
    CHECK(loadedDrawers[3]->front().handle()->modelId == "bar-256");
    CHECK(loadedDrawers[0]->front().handle()->modelId.empty()); // workshop default
}

TEST_CASE("Parts left out of the cutting plan and drawer box choices survive save and load",
          "[storage][project]")
{
    auto chest = createProduct(CatalogItem::ChestOfDrawers);
    chest->carcass()->back().setInCuttingPlan(false);
    const auto drawers = chest->carcass()->interior().drawerStack()->drawers();
    for (const auto& part : drawers[1]->children()) {
        if (part->kind() == ComponentKind::DrawerBottom) {
            auto& bottom = static_cast<PanelComponent&>(*part);
            bottom.setInCuttingPlan(false);
            bottom.setName("Hazır dip");
        } else if (part->kind() == ComponentKind::DrawerBoxSide) {
            static_cast<PanelComponent&>(*part).setMaterialOverride(
                Material{"Suntalam 18", MaterialType::Chipboard, 18.0, true});
        }
    }
    chest->update();

    const auto loaded = loadProduct(saveProduct(*chest));
    requireSameGeometry(*chest, *loaded);
    CHECK_FALSE(loaded->carcass()->back().inCuttingPlan());
    CHECK(loaded->carcass()->leftSide().inCuttingPlan());
    const auto loadedDrawers = loaded->carcass()->interior().drawerStack()->drawers();
    int boxSides = 0;
    for (const auto& part : loadedDrawers[1]->children()) {
        const auto& panel = static_cast<const PanelComponent&>(*part);
        if (part->kind() == ComponentKind::DrawerBottom) {
            CHECK_FALSE(panel.inCuttingPlan());
            CHECK(panel.name() == "Hazır dip");
        } else if (part->kind() == ComponentKind::DrawerBoxSide) {
            CHECK(panel.material().thickness == 18.0);
            ++boxSides;
        } else {
            CHECK(panel.inCuttingPlan());
        }
    }
    CHECK(boxSides == 2);
    for (const auto& part : loadedDrawers[0]->children()) {
        CHECK(static_cast<const PanelComponent&>(*part).inCuttingPlan()); // the other drawers
    }
}

TEST_CASE("Broken files are rejected with a clear error", "[storage][errors]")
{
    using Catch::Matchers::ContainsSubstring;

    CHECK_THROWS_AS(loadProduct("{ not json"), StorageError);
    CHECK_THROWS_WITH(loadProduct(R"({"format": "something-else", "version": 1})"),
                      ContainsSubstring("Not a readycad-project file"));
    CHECK_THROWS_WITH(loadProduct(R"({"format": "readycad-project", "version": 99})"),
                      ContainsSubstring("Unsupported file version 99"));
    CHECK_THROWS_AS(loadProfile(R"({"format": "readycad-project", "version": 1})"), StorageError);

    // A section with shelves AND drawers is not a valid design.
    auto product = WardrobeBuilder().shelvesPerSection(2).doorsPerSection(0).build();
    std::string text = saveProduct(*product);
    const std::string marker = R"("type": "shelf")";
    const auto at = text.find(marker);
    REQUIRE(at != std::string::npos);
    text.insert(text.rfind('{', at), R"({"type": "drawers", "drawers": [{"front": {}}]}, )");
    CHECK_THROWS_AS(loadProduct(text), StorageError);
}

TEST_CASE("Files with Turkish names", "[storage][file]")
{
    const auto dir = std::filesystem::temp_directory_path() / "readycad_tests";
    std::filesystem::create_directories(dir);
    const auto path = dir / std::filesystem::path(u8"çekmeceli_şifonyer.rcproj");

    const auto chest = createProduct(CatalogItem::ChestOfDrawers);
    writeTextFile(path, saveProduct(*chest));
    const auto loaded = loadProduct(readTextFile(path));
    requireSameGeometry(*chest, *loaded);

    std::filesystem::remove(path);
    CHECK_THROWS_AS(readTextFile(path), StorageError);
}
