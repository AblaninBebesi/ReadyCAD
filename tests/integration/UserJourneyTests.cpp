// Stage 9, end to end: the user journey of the diploma task, programmatically, through the same
// functions the window calls —
//
//   workshop files -> a wardrobe from the catalogue -> its parameters and shelves -> drilling ->
//   save and open again -> cutting plan from the stock -> production package (PDF, DXF of parts
//   and sheets, CNC programs, cut list, summary) -> every file read back -> sheets taken out of
//   the stock, remnants added.
//
// Each step checks what the next one relies on, so a failure points at the step that broke.

#include "readycad/core/model/Queries.hpp"
#include "readycad/editor/Operations.hpp"
#include "readycad/i18n/Localization.hpp"
#include "readycad/production/Pipeline.hpp"
#include "readycad/production/Summary.hpp"
#include "readycad/storage/Storage.hpp"
#include "readycad/test/DxfReader.hpp"
#include "readycad/test/GCodeReader.hpp"
#include "readycad/test/PdfReader.hpp"
#include "readycad/test/TempFolder.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <algorithm>
#include <filesystem>
#include <string>

using namespace rc;
using Catch::Matchers::ContainsSubstring;

namespace {

std::string txt(i18n::TextId id)
{
    return std::string(i18n::text(id));
}

/// The parts of the cut list in a form that can be compared between two documents.
std::vector<std::string> cutListFacts(const editor::Document& doc)
{
    std::vector<std::string> facts;
    for (const detailing::CutListRow& r : doc.cutList()) {
        facts.push_back(std::string(core::toString(r.kind)) + " " + r.material.name + " "
                        + std::to_string(r.cut.length) + "x" + std::to_string(r.cut.width) + " x"
                        + std::to_string(r.quantity) + (r.inCuttingPlan ? "" : " out"));
    }
    return facts;
}

int holesOf(const core::Product& product)
{
    int holes = 0;
    for (const core::FurnitureComponent* c : core::collectPanels(product)) {
        holes += static_cast<int>(c->panel()->drillings().size());
    }
    return holes;
}

} // namespace

TEST_CASE("User journey: from the catalogue to the files for the workshop",
          "[integration][journey]")
{
    const rc::test::TempFolder home("journey");

    // --- 1. The workshop: its stock and its machines, in its own folder -----------------------
    const storage::WorkshopFiles files = storage::WorkshopFiles::in(home.path() / "ReadyCAD");
    const core::Material board = core::ConstructionRules{}.carcassMaterial;
    {
        nesting::StockManager stock;
        stock.add(nesting::Sheet{0, board, 2800, 2100, 8, false, {}});
        stock.add(nesting::Sheet{0, board, 1300, 900, 1, true, "eski iş"});
        storage::saveStockFile(stock, files.stock);
        storage::AppSettings settings;
        settings.nesting.machine = nesting::Machine::NestingCenter;
        settings.nesting.optimizer.populationSize = 8;
        settings.nesting.optimizer.generations = 4;
        storage::saveSettingsFile(settings, files.settings);
    }
    // What the program does at start.
    nesting::StockManager stock = storage::loadStockFile(files.stock);
    const storage::AppSettings settings = storage::loadSettingsFile(files.settings);
    REQUIRE(stock.sheets().size() == 2);

    // --- 2. A wardrobe from the catalogue, made to measure -------------------------------------
    editor::Document doc = editor::Document::fromCatalog(core::CatalogItem::WardrobeWithDrawers);
    editor::setProductName(doc, "Yatak odası dolabı");
    editor::setDimensions(doc, {1800, 2300, 600});
    // Undo/redo rebuild the tree, so parts are looked up again after them.
    const auto column = [&doc](std::size_t i) {
        return doc.product().carcass()->interior().subsections().at(i);
    };
    REQUIRE(column(1)->subsections().size() == 2); // drawers below, shelves and doors above

    // Shelves of the right column: 4, the second fixed with confirmats.
    editor::setShelfCount(doc, column(1)->subsections().back()->id(), 4);
    editor::setShelfMount(doc, column(1)->subsections().back()->shelves()[1]->id(),
                          core::ShelfMount::Confirmat);
    // Knobs on the doors of the left column.
    for (const core::Door* door : column(0)->subsections().back()->doors()) {
        editor::setHandle(doc, door->id(), core::HandlePlacement{"knob-30"});
    }
    // Three drawers instead of two — undone, then done again.
    editor::setDrawerCount(doc, column(0)->subsections().front()->id(), 3);
    doc.undo();
    CHECK(column(0)->subsections().front()->drawerStack()->count() == 2);
    doc.redo();
    CHECK(column(0)->subsections().front()->drawerStack()->count() == 3);
    // The back panel comes cut to size from the supplier.
    editor::setInCuttingPlan(doc, {doc.product().carcass()->back().id()}, false);

    // --- 3. Drilling and the cut list follow every change -------------------------------------
    CHECK(doc.product().dimensions().width == 1800.0);
    CHECK(column(1)->subsections().back()->shelves().size() == 4);
    CHECK(core::findOverlaps(doc.product()).empty());
    CHECK(doc.detailingReport().warnings.empty());
    CHECK(doc.detailingReport().joints > 0);
    CHECK(holesOf(doc.product()) == doc.detailingReport().drillings);
    int drawerFronts = 0;
    int parts = 0;
    for (const detailing::CutListRow& r : doc.cutList()) {
        parts += r.quantity;
        drawerFronts += r.kind == core::ComponentKind::DrawerFront ? r.quantity : 0;
        CHECK(r.inCuttingPlan == (r.kind != core::ComponentKind::BackPanel));
        CHECK(r.cut.length > 0.0);
        CHECK(r.cut.width > 0.0);
    }
    CHECK(drawerFronts == 5); // 3 + 2

    // --- 4. Saved and opened again: the same parts --------------------------------------------
    const auto project = home.path() / "Müşteri Yılmaz.rcproj";
    doc.saveAs(project);
    CHECK_FALSE(doc.isDirty());
    const editor::Document reopened = editor::Document::open(project);
    CHECK(cutListFacts(reopened) == cutListFacts(doc));
    CHECK(reopened.detailingReport().drillings == doc.detailingReport().drillings);
    CHECK(production::orderName(reopened) == "Müşteri Yılmaz");

    // --- 5. The cutting plan: two wardrobes from the stock ------------------------------------
    const production::Order order{reopened, 2};
    const nesting::NestingResult plan = production::planCutting(order, stock, settings.nesting);
    REQUIRE(plan.unplaced.empty());
    CHECK(plan.placedCount() == 2 * (parts - 1)); // without the back panels
    const auto usage = plan.usage();
    const auto carcass = std::find_if(usage.begin(), usage.end(), [&](const auto& u) {
        return nesting::sameMaterial(u.material, board);
    });
    REQUIRE(carcass != usage.end());
    CHECK(carcass->stock + carcass->remnants >= 1); // the stock is used before buying
    for (const nesting::NestingPart& p : plan.parts) {
        CHECK(reopened.cutList()[*p.cutListRow].kind != core::ComponentKind::BackPanel);
    }
    CHECK(production::planMatches(order, plan));

    // --- 6. The production package ------------------------------------------------------------
    production::PackageOptions options;
    options.documentation.date = "12.10.2026";
    options.documentation.company = "Atölye";
    const auto folder = production::newPackageFolder(home.path(), production::orderName(reopened));
    CHECK(folder.filename() == "Musteri_Yilmaz_URETIM");
    const production::Package package = production::writePackage(
        order, &plan, settings.cnc, options, exporting::PdfFonts{}, folder);

    // PDF: complete, with a page per part drawing and per sheet at least.
    const std::string pdf = rc::test::readFile(package.documents);
    CHECK(rc::test::isCompletePdf(pdf));
    CHECK(rc::test::pdfPageCount(pdf) >= reopened.cutList().size() + plan.layouts.size());

    // DXF of every part: readable, closed with EOF, with the cut contour.
    REQUIRE(package.partDrawings.size() == reopened.cutList().size());
    for (const auto& file : package.partDrawings) {
        INFO(file.filename().string());
        const auto pairs = rc::test::parseDxf(rc::test::readFile(file));
        CHECK(pairs.back().value == "EOF");
        CHECK_FALSE(rc::test::dxfObjects(pairs, "ENTITIES").empty());
    }
    // DXF of every sheet.
    REQUIRE(package.sheetDrawings.size() == plan.layouts.size());
    for (const auto& file : package.sheetDrawings) {
        const auto pairs = rc::test::parseDxf(rc::test::readFile(file));
        CHECK(pairs.back().value == "EOF");
    }

    // A CNC program per sheet: complete, inside the sheet, never deeper than the spoilboard cut,
    // only tools of the machine.
    REQUIRE(package.programs.size() == plan.layouts.size());
    std::vector<int> tools;
    core::Real widest = 0.0;
    for (const cam::Tool& t : settings.cnc.tools) {
        tools.push_back(t.number);
        widest = std::max(widest, t.diameter);
    }
    int drilled = 0;
    for (std::size_t i = 0; i < package.programs.size(); ++i) {
        INFO(package.programs[i].filename().string());
        const nesting::SheetLayout& sheet = plan.layouts[i];
        const rc::test::Replay r = rc::test::replay(rc::test::readFile(package.programs[i]));
        CHECK(r.lines.back() == "M30");
        for (const int t : r.tools) {
            CHECK(std::find(tools.begin(), tools.end(), t) != tools.end());
        }
        for (const rc::test::Move& m : r.moves) {
            if (!rc::test::known(m.to)) {
                continue;
            }
            CHECK(m.to.x >= -widest / 2.0 - 1e-6);
            CHECK(m.to.y >= -widest / 2.0 - 1e-6);
            CHECK(m.to.x <= sheet.sheet.length + widest / 2.0 + 1e-6);
            CHECK(m.to.y <= sheet.sheet.width + widest / 2.0 + 1e-6);
            CHECK(m.to.z >= -settings.cnc.spoilboardCut - 1e-6);
        }
        drilled += package.cnc[i].stats.holes;
    }
    CHECK(drilled > 0);

    // The cut list for the saw: quantities of two wardrobes, the back panel marked.
    const std::string csv = rc::test::readFile(package.cutListCsv);
    CHECK_THAT(csv, ContainsSubstring(txt(i18n::TextId::No)));

    // The summary: the order, the machine time, the work for other machines, the bought part.
    CHECK_THAT(package.summaryText, ContainsSubstring("Müşteri Yılmaz × 2"));
    CHECK_THAT(package.summaryText, ContainsSubstring(txt(i18n::TextId::TotalTime)));
    CHECK_THAT(package.summaryText, ContainsSubstring(txt(i18n::TextId::LeftoverEdge)));
    CHECK_THAT(package.summaryText, ContainsSubstring(txt(i18n::TextId::OutOfPlanTitle)));
    CHECK(std::filesystem::exists(folder / "OZET.txt"));

    // The design in the package is the design that was cut.
    const editor::Document fromPackage = editor::Document::open(package.project);
    CHECK(cutListFacts(fromPackage) == cutListFacts(reopened));

    // --- 7. The sheets are cut: out of the stock, remnants in -------------------------------
    stock.consume(plan, production::orderName(reopened) + " · 12.10.2026");
    storage::saveStockFile(stock, files.stock);
    const nesting::StockManager after = storage::loadStockFile(files.stock);
    int fullSheets = 0;
    int newRemnants = 0;
    for (const nesting::Sheet& s : after.sheets()) {
        if (nesting::sameMaterial(s.material, board)) {
            if (!s.remnant) {
                fullSheets += s.quantity;
            } else if (s.note == "Müşteri Yılmaz · 12.10.2026") {
                newRemnants += s.quantity;
            }
        }
    }
    CHECK(fullSheets == 8 - carcass->stock);
    CHECK(newRemnants == carcass->newRemnants);

    // A second package of the same order never lands in the first one's folder.
    CHECK(production::newPackageFolder(home.path(), production::orderName(reopened)).filename()
          == "Musteri_Yilmaz_URETIM_2");
}
