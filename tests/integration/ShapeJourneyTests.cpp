// Shapes, end to end: a wardrobe with a notch for the skirting board in both walls, a cable hole
// and rounded front corners on a shelf — saved, opened, cut on both machines: the drawings show
// the real outlines, the nesting centre cuts the hole from the inside before the shelf comes
// loose, the saw leaves the shapes for afterwards.

#include "readycad/editor/Operations.hpp"
#include "readycad/i18n/Localization.hpp"
#include "readycad/production/Pipeline.hpp"
#include "readycad/production/Summary.hpp"
#include "readycad/storage/Storage.hpp"
#include "readycad/test/DxfReader.hpp"
#include "readycad/test/GCodeReader.hpp"
#include "readycad/test/TempFolder.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <algorithm>
#include <filesystem>
#include <string>

using namespace rc;
using Catch::Matchers::ContainsSubstring;

TEST_CASE("Shape journey: notches, a cable hole and rounded corners down to the CNC program",
          "[integration][journey][shape]")
{
    const rc::test::TempFolder home("shape_journey");

    // --- 1. The design ------------------------------------------------------------------------
    editor::Document doc = editor::Document::fromCatalog(core::CatalogItem::Wardrobe);
    editor::setProductName(doc, "Koridor dolabı");
    const core::Carcass& carcass = *doc.product().carcass();
    for (const core::ComponentId wall : {carcass.leftSide().id(), carcass.rightSide().id()}) {
        // The rear bottom corner of both walls (the right wall's rear edge is L1 too).
        editor::setShape(doc, wall, {core::CornerNotch{core::PanelCorner::L1W1, 100.0, 15.0}});
    }
    editor::setShape(doc, doc.product().carcass()->interior().shelves()[1]->id(),
                     {core::CornerRound{core::PanelCorner::L2W1, 60.0},
                      core::CornerRound{core::PanelCorner::L2W2, 60.0},
                      core::RoundHole{core::PanelCorner::L1W1, 381.0, 80.0, 60.0}});
    CHECK(doc.shapeConflicts().empty());
    CHECK(doc.overlaps().empty());

    // --- 2. Saved and opened again ------------------------------------------------------------
    const auto project = home.path() / "Koridor dolabı.rcproj";
    doc.saveAs(project);
    const editor::Document reopened = editor::Document::open(project);
    CHECK(storage::saveProduct(reopened.product()) == storage::saveProduct(doc.product()));

    // --- 3. The nesting centre ----------------------------------------------------------------
    nesting::NestingSettings settings;
    settings.machine = nesting::Machine::NestingCenter;
    settings.optimizer.populationSize = 6;
    settings.optimizer.generations = 3;
    const production::Order order{reopened, 1};
    const nesting::NestingResult plan = production::planCutting(order, nesting::StockManager{}, settings);
    REQUIRE(plan.unplaced.empty());
    const auto folder = production::newPackageFolder(home.path(), "Koridor dolabı");
    const production::Package package = production::writePackage(
        order, &plan, cam::defaultProfile(), {}, exporting::PdfFonts{}, folder);

    // The walls' drawings have six corners, the shelf's outline its rounded corners and a hole.
    int walls = 0;
    int shelvesWithHole = 0;
    for (const auto& file : package.partDrawings) {
        const auto entities = rc::test::dxfObjects(rc::test::parseDxf(rc::test::readFile(file)), "ENTITIES");
        const std::string name = file.filename().string();
        int cutouts = 0;
        std::size_t contourPoints = 0;
        for (std::size_t i = 0; i < entities.size(); ++i) {
            if (entities[i].type == "POLYLINE" && entities[i].get(8) == "CUTOUT") {
                ++cutouts;
            }
            if (entities[i].type == "POLYLINE" && entities[i].get(8) == "CONTOUR") {
                for (std::size_t j = i + 1; j < entities.size() && entities[j].type == "VERTEX"; ++j) {
                    ++contourPoints;
                }
            }
        }
        if (name.find("Yan_panel") != std::string::npos) {
            ++walls;
            CHECK(contourPoints == 6); // the notch: two more corners
        }
        shelvesWithHole += name.find("Raf") != std::string::npos && cutouts == 1 ? 1 : 0;
    }
    CHECK(walls == 2);
    CHECK(shelvesWithHole == 1);

    // The program of the sheet with the shelf: the 60 mm hole before the shelf's outline.
    bool found = false;
    for (const auto& file : package.programs) {
        const std::string program = rc::test::readFile(file);
        const std::size_t hole = program.find("Raf 60x60");
        if (hole == std::string::npos) {
            continue;
        }
        found = true;
        const std::size_t outline = program.find("Raf ", hole + 1);
        CHECK(outline != std::string::npos);
        const rc::test::Replay replay = rc::test::replay(program);
        CHECK(replay.lines.back() == "M30");
        for (const rc::test::Move& m : replay.moves) {
            if (rc::test::known(m.to)) {
                CHECK(m.to.z >= -cam::defaultProfile().spoilboardCut - 1e-6);
            }
        }
    }
    CHECK(found);
    for (const cam::SheetProgram& p : package.cnc) {
        for (const cam::Leftover& l : p.leftovers) {
            CHECK(l.noTool == 0); // the router reaches every notch and the hole
        }
    }

    // --- 4. The panel saw: the shapes come after it -------------------------------------------
    settings.machine = nesting::Machine::PanelSaw;
    const nesting::NestingResult saw = production::planCutting(order, nesting::StockManager{}, settings);
    const std::string summary = production::summaryText(order, &saw, production::Package{});
    CHECK_THAT(summary, ContainsSubstring(std::string(i18n::text(i18n::TextId::AfterSawTitle))));
    CHECK_THAT(summary, ContainsSubstring("Yan panel (sol)"));
    CHECK_THAT(summary, ContainsSubstring("Yan panel (sağ)"));
}
