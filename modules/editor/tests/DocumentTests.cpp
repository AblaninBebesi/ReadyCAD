// Stage 5: the Document — undo/redo, merging of slider drags, selection, files, failed edits.

#include "readycad/editor/Document.hpp"
#include "readycad/editor/Operations.hpp"
#include "readycad/storage/Storage.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <stdexcept>

using namespace rc::core;
using namespace rc::editor;
using Catch::Approx;

namespace {

std::string state(const Document& doc)
{
    return rc::storage::saveProduct(doc.product());
}

Section& interior(Document& doc)
{
    return doc.product().carcass()->interior();
}

} // namespace

TEST_CASE("A new document is detailed and clean", "[editor][document]")
{
    const Document doc = Document::fromCatalog(CatalogItem::Wardrobe);
    CHECK(doc.detailingReport().drillings > 0);
    CHECK(doc.detailingReport().warnings.empty());
    CHECK_FALSE(doc.cutList().empty());
    CHECK_FALSE(doc.isDirty());
    CHECK_FALSE(doc.canUndo());
}

TEST_CASE("Undo and redo restore the exact state", "[editor][document][undo]")
{
    Document doc = Document::fromCatalog(CatalogItem::Wardrobe);
    const std::string original = state(doc);

    setDimensions(doc, {900, 2000, 560});
    doc.endMerge();
    setShelfCount(doc, interior(doc).id(), 6);
    const std::string edited = state(doc);
    CHECK(doc.isDirty());
    CHECK(doc.undoDescription() == "Shelves");

    doc.undo();
    CHECK(interior(doc).shelves().size() == 4);
    doc.undo();
    CHECK(state(doc) == original);
    CHECK_FALSE(doc.canUndo());

    doc.redo();
    doc.redo();
    CHECK(state(doc) == edited);
    CHECK_FALSE(doc.canRedo());

    SECTION("a new edit clears the redo history")
    {
        doc.undo();
        setDoorCount(doc, interior(doc).id(), 1);
        CHECK_FALSE(doc.canRedo());
    }
}

TEST_CASE("Dragging a slider is one undo step", "[editor][document][undo]")
{
    Document doc = Document::fromCatalog(CatalogItem::Wardrobe);

    // ImGui sends a new value every frame while the mouse is held down...
    for (const Real width : {810.0, 830.0, 860.0, 900.0}) {
        setDimensions(doc, {width, 2000, 560});
    }
    doc.endMerge(); // ...and the UI calls endMerge() when the slider is released.
    setDimensions(doc, {950, 2000, 560});

    doc.undo();
    CHECK(doc.product().dimensions().width == 900.0);
    doc.undo();
    CHECK(doc.product().dimensions().width == 800.0);
    CHECK_FALSE(doc.canUndo());
}

TEST_CASE("Derived data follows every change", "[editor][document]")
{
    Document doc = Document::fromCatalog(CatalogItem::Wardrobe);
    const auto revision = doc.revision();
    const Real sideCutWidth = doc.cutList().front().cut.width; // side wall: depth 560 -> 559

    setDimensions(doc, {800, 2000, 600});

    CHECK(doc.revision() > revision);
    CHECK(doc.cutList().front().cut.width == Approx(sideCutWidth + 40.0));
    // Drilling was regenerated for the new geometry (relayout cleared it).
    CHECK_FALSE(doc.product().carcass()->top().panel()->drillings().empty());
}

TEST_CASE("The hardware list follows the shelves", "[editor][document]")
{
    Document doc = Document::fromCatalog(CatalogItem::Wardrobe);
    const auto supports = [&doc] {
        int n = 0;
        for (const rc::detailing::HardwareItem& item : doc.hardware()) {
            n += item.kind == HardwareKind::ShelfSupport ? item.quantity : 0;
        }
        return n;
    };
    const int before = supports();
    setShelfCount(doc, interior(doc).id(), static_cast<int>(interior(doc).shelves().size()) + 1);
    CHECK(supports() == before + 4);
    doc.undo();
    CHECK(supports() == before);
}

TEST_CASE("Selection survives undo although the objects are rebuilt", "[editor][document]")
{
    Document doc = Document::fromCatalog(CatalogItem::Wardrobe);
    const Shelf* second = interior(doc).shelves()[1];
    doc.select(second->id());

    setDimensions(doc, {900, 2000, 560});
    doc.undo();

    REQUIRE(doc.selected() != nullptr);
    CHECK(doc.selected() != second);                    // a new object after the rebuild...
    CHECK(doc.selected() == interior(doc).shelves()[1]); // ...at the same place in the tree
}

TEST_CASE("A failed edit leaves no trace", "[editor][document]")
{
    Document doc = Document::fromCatalog(CatalogItem::Wardrobe);
    const std::string before = state(doc);

    CHECK_THROWS_AS(setDimensions(doc, {-5, 2000, 560}), std::invalid_argument);
    CHECK_THROWS_AS(setDrawerCount(doc, interior(doc).id(), 2), std::logic_error); // has shelves
    CHECK_THROWS_AS(setShelfCount(doc, ComponentId{987654}, 2), std::invalid_argument);
    CHECK_THROWS_AS(removeComponent(doc, doc.product().carcass()->leftSide().id()),
                    std::logic_error);

    CHECK(state(doc) == before);
    CHECK_FALSE(doc.canUndo());
    CHECK_FALSE(doc.isDirty());
}

TEST_CASE("Save, open and dirty flag", "[editor][document][file]")
{
    const auto path = std::filesystem::temp_directory_path() / "readycad_tests" / "dolap.rcproj";
    std::filesystem::create_directories(path.parent_path());

    Document doc = Document::fromCatalog(CatalogItem::WardrobeWithDrawers);
    CHECK_THROWS_AS(doc.save(), rc::storage::StorageError); // no file name yet
    setDimensions(doc, {1700, 2200, 600});
    doc.saveAs(path);
    CHECK_FALSE(doc.isDirty());
    CHECK(doc.path() == path);

    const Document reopened = Document::open(path);
    CHECK(state(reopened) == state(doc));
    CHECK(reopened.detailingReport().drillings == doc.detailingReport().drillings);
    std::filesystem::remove(path);
}
